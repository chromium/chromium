// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom-shared.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_service.h"
#include "crypto/secure_hash.h"

namespace {

constexpr base::TimeDelta kContactDownloadPeriod =
    base::TimeDelta::FromHours(12);
constexpr base::TimeDelta kContactDownloadRpcTimeout =
    base::TimeDelta::FromSeconds(60);

// Removes contact IDs from the allowlist if they are not in |contacts|.
std::set<std::string> RemoveNonexistentContactsFromAllowlist(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  std::set<std::string> new_allowed_contact_ids;
  for (const nearbyshare::proto::ContactRecord& contact : contacts) {
    if (base::Contains(allowed_contact_ids, contact.id()))
      new_allowed_contact_ids.insert(contact.id());
  }
  return new_allowed_contact_ids;
}

// Converts a list of ContactRecord protos, along with the allowlist, into a
// list of Contact protos.
std::vector<nearbyshare::proto::Contact> ContactRecordsToContacts(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contact_records) {
  std::vector<nearbyshare::proto::Contact> contacts;
  for (const auto& contact_record : contact_records) {
    bool is_selected = base::Contains(allowed_contact_ids, contact_record.id());
    for (const auto& identifier : contact_record.identifiers()) {
      nearbyshare::proto::Contact contact;
      contact.mutable_identifier()->CopyFrom(identifier);
      contact.set_is_selected(is_selected);
      contacts.push_back(contact);
    }
  }
  return contacts;
}

// Creates a hex-encoded hash of the contact data, implicitly including the
// allowlist, to be sent to the Nearby Share server. This hash is persisted and
// used to detect any changes to the user's contact list or allowlist since the
// last successful upload to the server.
std::string ComputeHash(
    const std::vector<nearbyshare::proto::Contact>& contacts) {
  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);

  for (const nearbyshare::proto::Contact& contact : contacts) {
    std::string serialized = contact.SerializeAsString();
    hasher->Update(serialized.data(), serialized.size());
  }

  std::vector<uint8_t> hash(hasher->GetHashLength());
  hasher->Finish(hash.data(), hash.size());

  return base::HexEncode(hash);
}

nearby_share::mojom::ContactIdentifierPtr ProtoToMojo(
    const nearbyshare::proto::Contact_Identifier& identifier) {
  nearby_share::mojom::ContactIdentifierPtr identifier_ptr =
      nearby_share::mojom::ContactIdentifier::New();
  switch (identifier.identifier_case()) {
    case nearbyshare::proto::Contact_Identifier::IdentifierCase::kAccountName:
      identifier_ptr->set_account_name(identifier.account_name());
      break;
    case nearbyshare::proto::Contact_Identifier::IdentifierCase::
        kObfuscatedGaia:
      identifier_ptr->set_obfuscated_gaia(identifier.obfuscated_gaia());
      break;
    case nearbyshare::proto::Contact_Identifier::IdentifierCase::kPhoneNumber:
      identifier_ptr->set_phone_number(identifier.phone_number());
      break;
    case nearbyshare::proto::Contact_Identifier::IdentifierCase::
        IDENTIFIER_NOT_SET:
      NOTREACHED();
      break;
  }
  return identifier_ptr;
}

nearby_share::mojom::ContactRecordPtr ProtoToMojo(
    const nearbyshare::proto::ContactRecord& contact_record) {
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

std::vector<nearby_share::mojom::ContactRecordPtr> ProtoToMojo(
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  std::vector<nearby_share::mojom::ContactRecordPtr> mojo_contacts;
  mojo_contacts.reserve(contacts.size());
  for (const auto& contact_record : contacts) {
    mojo_contacts.push_back(ProtoToMojo(contact_record));
  }
  return mojo_contacts;
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
    NearbyShareLocalDeviceDataManager* local_device_data_manager) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, http_client_factory,
                                         local_device_data_manager);
  }
  return base::WrapUnique(new NearbyShareContactManagerImpl(
      pref_service, http_client_factory, local_device_data_manager));
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
    NearbyShareLocalDeviceDataManager* local_device_data_manager)
    : pref_service_(pref_service),
      http_client_factory_(http_client_factory),
      local_device_data_manager_(local_device_data_manager),
      contact_download_and_upload_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kContactDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName,
              pref_service_,
              base::BindRepeating(
                  &NearbyShareContactManagerImpl::OnContactsDownloadRequested,
                  base::Unretained(this)))) {}

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
  contact_download_and_upload_scheduler_->Start();
}

void NearbyShareContactManagerImpl::OnStop() {
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
       pref_service_->Get(prefs::kNearbySharingAllowedContactsPrefName)
           ->GetList()) {
    allowlist.insert(id.GetString());
  }
  return allowlist;
}

void NearbyShareContactManagerImpl::OnContactsDownloadRequested() {
  NS_LOG(VERBOSE) << __func__ << ": Nearby Share contacts download requested.";

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
    std::vector<nearbyshare::proto::ContactRecord> contacts) {
  contact_downloader_.reset();

  NS_LOG(VERBOSE) << __func__ << ": Nearby Share download of "
                  << contacts.size() << " contacts succeeded.";

  // Remove contacts from the allowlist that are not in the contact list.
  SetAllowlist(
      RemoveNonexistentContactsFromAllowlist(GetAllowedContacts(), contacts));

  // Notify observers that the contact list was downloaded.
  std::set<std::string> allowed_contact_ids = GetAllowedContacts();
  NotifyContactsDownloaded(allowed_contact_ids, contacts);
  NotifyMojoObserverContactsDownloaded(allowed_contact_ids, contacts);

  // Only request a contacts upload if the contact list or allowlist has changed
  // since the last successful upload.
  std::vector<nearbyshare::proto::Contact> contacts_to_upload =
      ContactRecordsToContacts(GetAllowedContacts(), contacts);
  std::string contact_upload_hash = ComputeHash(contacts_to_upload);
  if (contact_upload_hash ==
      pref_service_->GetString(
          prefs::kNearbySharingContactUploadHashPrefName)) {
    contact_download_and_upload_scheduler_->HandleResult(/*success=*/true);
    return;
  }

  NS_LOG(VERBOSE) << __func__
                  << ": Contact list or allowlist changed since last "
                  << "successful upload to the Nearby Share server. "
                  << "Starting contacts upload.";
  local_device_data_manager_->UploadContacts(
      std::move(contacts_to_upload),
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsUploadFinished,
                     weak_ptr_factory_.GetWeakPtr(), contact_upload_hash));
}

void NearbyShareContactManagerImpl::OnContactsDownloadFailure() {
  contact_downloader_.reset();

  NS_LOG(WARNING) << __func__ << ": Nearby Share contacts download failed.";

  // Notify mojo remotes.
  for (auto& remote : observers_set_) {
    remote->OnContactsDownloadFailed();
  }

  contact_download_and_upload_scheduler_->HandleResult(/*success=*/false);
}

void NearbyShareContactManagerImpl::OnContactsUploadFinished(
    const std::string& contact_upload_hash,
    bool success) {
  NS_LOG(VERBOSE) << __func__ << ": Upload of contacts to Nearby Share server "
                  << (success ? "succeeded." : "failed.")
                  << " Contact upload hash: " << contact_upload_hash;
  if (success) {
    pref_service_->SetString(prefs::kNearbySharingContactUploadHashPrefName,
                             contact_upload_hash);
    NotifyContactsUploaded(/*did_contacts_change_since_last_upload=*/true);
  }

  contact_download_and_upload_scheduler_->HandleResult(success);
}

bool NearbyShareContactManagerImpl::SetAllowlist(
    const std::set<std::string>& new_allowlist) {
  if (new_allowlist == GetAllowedContacts())
    return false;

  base::Value allowlist_value(base::Value::Type::LIST);
  for (const std::string& id : new_allowlist) {
    allowlist_value.Append(id);
  }
  pref_service_->Set(prefs::kNearbySharingAllowedContactsPrefName,
                     std::move(allowlist_value));

  return true;
}

void NearbyShareContactManagerImpl::NotifyMojoObserverContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  if (observers_set_.empty()) {
    return;
  }

  // Mojo doesn't have sets, so we have to copy to an array.
  std::vector<std::string> allowed_contact_ids_vector(
      allowed_contact_ids.begin(), allowed_contact_ids.end());

  // Notify mojo remotes.
  for (auto& remote : observers_set_) {
    remote->OnContactsDownloaded(allowed_contact_ids_vector,
                                 ProtoToMojo(contacts));
  }
}
