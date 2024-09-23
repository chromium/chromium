// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contacts_sorter.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "crypto/secure_hash.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

constexpr base::TimeDelta kContactUploadPeriod = base::Hours(24);
constexpr base::TimeDelta kContactDownloadPeriod = base::Hours(12);
constexpr base::TimeDelta kContactDownloadRpcTimeout = base::Seconds(60);

// Removes contact IDs from the allowlist if they are not in |contacts|.
std::set<std::string> RemoveNonexistentContactsFromAllowlist(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts) {
  std::set<std::string> new_allowed_contact_ids;
  for (const nearby::sharing::proto::ContactRecord& contact : contacts) {
    if (base::Contains(allowed_contact_ids, contact.id()))
      new_allowed_contact_ids.insert(contact.id());
  }
  return new_allowed_contact_ids;
}

// Converts a list of ContactRecord protos, along with the allowlist, into a
// list of Contact protos.
std::vector<nearby::sharing::proto::Contact> ContactRecordsToContacts(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contact_records) {
  std::vector<nearby::sharing::proto::Contact> contacts;
  for (const auto& contact_record : contact_records) {
    bool is_selected = base::Contains(allowed_contact_ids, contact_record.id());
    for (const auto& identifier : contact_record.identifiers()) {
      nearby::sharing::proto::Contact contact;
      contact.mutable_identifier()->CopyFrom(identifier);
      contact.set_is_selected(is_selected);
      contacts.push_back(contact);
    }
  }
  return contacts;
}

nearby::sharing::proto::Contact CreateLocalContact(
    const std::string& profile_user_name) {
  nearby::sharing::proto::Contact contact;
  contact.mutable_identifier()->set_account_name(profile_user_name);
  // Always consider your own account a selected contact.
  contact.set_is_selected(true);
  return contact;
}

// Creates a hex-encoded hash of the contact data, implicitly including the
// allowlist, to be sent to the Nearby Share server. This hash is persisted and
// used to detect any changes to the user's contact list or allowlist since the
// last successful upload to the server. The hash is invariant under the
// ordering of |contacts|.
std::string ComputeHash(
    const std::vector<nearby::sharing::proto::Contact>& contacts) {
  // To ensure that the hash is invariant under ordering of input |contacts|,
  // add all serialized protos to an ordered set. Then, incrementally calculate
  // the hash as we itereate through the set.
  std::set<std::string> serialized_contacts_set;
  for (const nearby::sharing::proto::Contact& contact : contacts) {
    serialized_contacts_set.insert(contact.SerializeAsString());
  }
  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);
  for (const std::string& serialized_contact : serialized_contacts_set) {
    hasher->Update(base::as_byte_span(serialized_contact));
  }
  std::vector<uint8_t> hash(hasher->GetHashLength());
  hasher->Finish(hash);

  return base::HexEncode(hash);
}

nearby_share::mojom::ContactIdentifierPtr ProtoToMojo(
    const nearby::sharing::proto::Contact_Identifier& identifier) {
  switch (identifier.identifier_case()) {
    case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
        kAccountName:
      return nearby_share::mojom::ContactIdentifier::NewAccountName(
          identifier.account_name());
    case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
        kObfuscatedGaia:
      return nearby_share::mojom::ContactIdentifier::NewObfuscatedGaia(
          identifier.obfuscated_gaia());
    case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
        kPhoneNumber:
      return nearby_share::mojom::ContactIdentifier::NewPhoneNumber(
          identifier.phone_number());
    case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
        IDENTIFIER_NOT_SET:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

nearby_share::mojom::ContactRecordPtr ProtoToMojo(
    const nearby::sharing::proto::ContactRecord& contact_record) {
  nearby_share::mojom::ContactRecordPtr contact_record_ptr =
      nearby_share::mojom::ContactRecord::New();
  contact_record_ptr->id = contact_record.id();
  contact_record_ptr->person_name = contact_record.person_name();
  contact_record_ptr->image_url = GURL(contact_record.image_url());
  for (const auto& identifier : contact_record.identifiers()) {
    contact_record_ptr->identifiers.push_back(ProtoToMojo(identifier));
  }
  return contact_record_ptr;
}

// Note: This conversion preserves the ordering of |contacts|.
std::vector<nearby_share::mojom::ContactRecordPtr> ProtoToMojo(
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts) {
  std::vector<nearby_share::mojom::ContactRecordPtr> mojo_contacts;
  mojo_contacts.reserve(contacts.size());
  for (const auto& contact_record : contacts) {
    mojo_contacts.push_back(ProtoToMojo(contact_record));
  }
  return mojo_contacts;
}

void RecordAllowlistMetrics(size_t num_contacts,
                            size_t num_allowed_contacts,
                            PrefService* pref_service) {
  // Only record metrics if the user is in selected-contacts visibility mode.
  // Note: We should really use NearbyShareSettings to get the visibility.
  // Because this is just for metrics, we read the pref directly for simplicity.
  nearby_share::mojom::Visibility visibility =
      static_cast<nearby_share::mojom::Visibility>(pref_service->GetInteger(
          prefs::kNearbySharingBackgroundVisibilityName));
  if (visibility != nearby_share::mojom::Visibility::kSelectedContacts)
    return;

  base::UmaHistogramCounts10000("Nearby.Share.Contacts.NumContacts.Selected",
                                num_allowed_contacts);

  if (num_contacts != 0) {
    base::UmaHistogramPercentage(
        "Nearby.Share.Contacts.PercentSelected",
        std::lround(100.0f * num_allowed_contacts / num_contacts));
  }
}

}  // namespace

// static
NearbyShareContactManagerImpl::Factory*
    NearbyShareContactManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactManager>
NearbyShareContactManagerImpl::Factory::Create(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, http_client_factory,
                                         local_device_data_manager,
                                         profile_info_provider);
  }
  return base::WrapUnique(new NearbyShareContactManagerImpl(
      pref_service, http_client_factory, local_device_data_manager,
      profile_info_provider));
}

// static
void NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactManagerImpl::Factory::~Factory() = default;

NearbyShareContactManagerImpl::NearbyShareContactManagerImpl(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareProfileInfoProvider* profile_info_provider)
    : pref_service_(pref_service),
      http_client_factory_(http_client_factory),
      local_device_data_manager_(local_device_data_manager),
      profile_info_provider_(profile_info_provider),
      periodic_contact_upload_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreatePeriodicScheduler(
              kContactUploadPeriod,
              /*retry_failures=*/false,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerPeriodicContactUploadPrefName,
              pref_service_,
              base::BindRepeating(&NearbyShareContactManagerImpl::
                                      OnPeriodicContactsUploadRequested,
                                  base::Unretained(this)),
              Feature::NS)),
      contact_download_and_upload_scheduler_(
          ash::nearby::NearbySchedulerFactory::CreatePeriodicScheduler(
              kContactDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName,
              pref_service_,
              base::BindRepeating(
                  &NearbyShareContactManagerImpl::OnContactsDownloadRequested,
                  base::Unretained(this)),
              Feature::NS)) {}

NearbyShareContactManagerImpl::~NearbyShareContactManagerImpl() = default;

void NearbyShareContactManagerImpl::DownloadContacts() {
  // Make sure the scheduler is running so we can retrieve contacts while
  // onboarding.
  Start();

  contact_download_and_upload_scheduler_->MakeImmediateRequest();
}

void NearbyShareContactManagerImpl::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  // If the allowlist changed, re-upload contacts to Nearby server.
  if (SetAllowlist(allowed_contact_ids))
    contact_download_and_upload_scheduler_->MakeImmediateRequest();
}

void NearbyShareContactManagerImpl::OnStart() {
  periodic_contact_upload_scheduler_->Start();
  contact_download_and_upload_scheduler_->Start();
}

void NearbyShareContactManagerImpl::OnStop() {
  periodic_contact_upload_scheduler_->Stop();
  contact_download_and_upload_scheduler_->Stop();
}

void NearbyShareContactManagerImpl::Bind(
    mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void NearbyShareContactManagerImpl::AddDownloadContactsObserver(
    ::mojo::PendingRemote<nearby_share::mojom::DownloadContactsObserver>
        observer) {
  observers_set_.Add(std::move(observer));
}

std::set<std::string> NearbyShareContactManagerImpl::GetAllowedContacts()
    const {
  std::set<std::string> allowlist;
  for (const base::Value& id :
       pref_service_->GetList(prefs::kNearbySharingAllowedContactsPrefName)) {
    allowlist.insert(id.GetString());
  }
  return allowlist;
}

void NearbyShareContactManagerImpl::OnPeriodicContactsUploadRequested() {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Periodic Nearby Share contacts upload requested. "
      << "Upload will occur after next contacts download.";
}

void NearbyShareContactManagerImpl::OnContactsDownloadRequested() {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Nearby Share contacts download requested.";

  DCHECK(!contact_downloader_);
  contact_downloader_ = NearbyShareContactDownloaderImpl::Factory::Create(
      local_device_data_manager_->GetId(), kContactDownloadRpcTimeout,
      http_client_factory_,
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsDownloadSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsDownloadFailure,
                     base::Unretained(this)));
  contact_downloader_->Run();
}

void NearbyShareContactManagerImpl::OnContactsDownloadSuccess(
    std::vector<nearby::sharing::proto::ContactRecord> contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  contact_downloader_.reset();

  CD_LOG(INFO, Feature::NS) << __func__ << ": Nearby Share download of "
                            << contacts.size() << " contacts succeeded.";

  // Remove contacts from the allowlist that are not in the contact list.
  SetAllowlist(
      RemoveNonexistentContactsFromAllowlist(GetAllowedContacts(), contacts));

  // Notify observers that the contact list was downloaded.
  std::set<std::string> allowed_contact_ids = GetAllowedContacts();
  RecordAllowlistMetrics(contacts.size(), allowed_contact_ids.size(),
                         pref_service_);
  NotifyAllObserversContactsDownloaded(allowed_contact_ids, contacts,
                                       num_unreachable_contacts_filtered_out);

  std::vector<nearby::sharing::proto::Contact> contacts_to_upload =
      ContactRecordsToContacts(GetAllowedContacts(), contacts);

  // Enable cross-device self-share by adding your account to the list of
  // contacts. It is also marked as a selected contact.
  std::optional<std::string> user_name =
      profile_info_provider_->GetProfileUserName();
  base::UmaHistogramBoolean("Nearby.Share.Contacts.CanGetProfileUserName",
                            user_name.has_value());
  if (!user_name) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Profile user name is not valid; could not "
        << "add self to list of contacts to upload.";
  } else {
    contacts_to_upload.push_back(CreateLocalContact(*user_name));
  }

  std::string contact_upload_hash = ComputeHash(contacts_to_upload);
  bool did_contacts_change_since_last_upload =
      contact_upload_hash !=
      pref_service_->GetString(prefs::kNearbySharingContactUploadHashPrefName);
  if (did_contacts_change_since_last_upload) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Contact list or allowlist changed since last "
        << "successful upload to the Nearby Share server.";
  }

  // Request a contacts upload if the contact list or allowlist has changed
  // since the last successful upload. Also request an upload periodically.
  if (did_contacts_change_since_last_upload ||
      periodic_contact_upload_scheduler_->IsWaitingForResult()) {
    local_device_data_manager_->UploadContacts(
        std::move(contacts_to_upload),
        base::BindOnce(&NearbyShareContactManagerImpl::OnContactsUploadFinished,
                       weak_ptr_factory_.GetWeakPtr(),
                       did_contacts_change_since_last_upload,
                       contact_upload_hash));
    return;
  }

  // No upload is needed.
  contact_download_and_upload_scheduler_->HandleResult(/*success=*/true);
}

void NearbyShareContactManagerImpl::OnContactsDownloadFailure() {
  contact_downloader_.reset();

  CD_LOG(WARNING, Feature::NS)
      << __func__ << ": Nearby Share contacts download failed.";

  // Notify mojo remotes.
  for (auto& remote : observers_set_) {
    remote->OnContactsDownloadFailed();
  }

  contact_download_and_upload_scheduler_->HandleResult(/*success=*/false);
}

void NearbyShareContactManagerImpl::OnContactsUploadFinished(
    bool did_contacts_change_since_last_upload,
    const std::string& contact_upload_hash,
    bool success) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Upload of contacts to Nearby Share server "
      << (success ? "succeeded." : "failed.")
      << " Contact upload hash: " << contact_upload_hash;
  if (success) {
    // Only resolve the periodic upload request on success; let the
    // download-and-upload scheduler handle any failure retries. The periodic
    // upload scheduler will remember that it has an outstanding request even
    // after reboot.
    if (periodic_contact_upload_scheduler_->IsWaitingForResult()) {
      periodic_contact_upload_scheduler_->HandleResult(success);
    }

    pref_service_->SetString(prefs::kNearbySharingContactUploadHashPrefName,
                             contact_upload_hash);
    NotifyContactsUploaded(did_contacts_change_since_last_upload);
  }

  contact_download_and_upload_scheduler_->HandleResult(success);
}

bool NearbyShareContactManagerImpl::SetAllowlist(
    const std::set<std::string>& new_allowlist) {
  if (new_allowlist == GetAllowedContacts())
    return false;

  base::Value::List allowlist_value;
  for (const std::string& id : new_allowlist) {
    allowlist_value.Append(id);
  }
  pref_service_->SetList(prefs::kNearbySharingAllowedContactsPrefName,
                         std::move(allowlist_value));

  return true;
}

void NearbyShareContactManagerImpl::NotifyAllObserversContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  // Sort the contacts before sending the list to observers.
  std::vector<nearby::sharing::proto::ContactRecord> sorted_contacts = contacts;
  SortNearbyShareContactRecords(&sorted_contacts);

  // First, notify NearbyShareContactManager::Observers.
  // Note: These are direct observers of the NearbyShareContactManager base
  // class, distinct from the mojo remote observers that we notify below.
  NotifyContactsDownloaded(allowed_contact_ids, sorted_contacts,
                           num_unreachable_contacts_filtered_out);

  // Next, notify mojo remote observers.
  if (observers_set_.empty()) {
    return;
  }

  // Mojo doesn't have sets, so we have to copy to an array.
  std::vector<std::string> allowed_contact_ids_vector(
      allowed_contact_ids.begin(), allowed_contact_ids.end());

  for (auto& remote : observers_set_) {
    remote->OnContactsDownloaded(allowed_contact_ids_vector,
                                 ProtoToMojo(sorted_contacts),
                                 num_unreachable_contacts_filtered_out);
  }
}
