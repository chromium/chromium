// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
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
#include "components/prefs/pref_service.h"

namespace {

constexpr base::TimeDelta kContactDownloadPeriod =
    base::TimeDelta::FromHours(1);
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
      contact_download_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              kContactDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerContactDownloadPrefName,
              pref_service_,
              base::BindRepeating(
                  &NearbyShareContactManagerImpl::OnContactsDownloadRequested,
                  base::Unretained(this)))),
      contact_upload_scheduler_(
          NearbyShareSchedulerFactory::CreateOnDemandScheduler(
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerContactUploadPrefName,
              pref_service_,
              base::BindRepeating(
                  &NearbyShareContactManagerImpl::OnContactsUploadRequested,
                  base::Unretained(this)))) {}

NearbyShareContactManagerImpl::~NearbyShareContactManagerImpl() = default;

void NearbyShareContactManagerImpl::DownloadContacts(
    bool only_download_if_changed) {
  // A request for a full download always takes priority.
  if (!only_download_if_changed)
    only_download_if_changed_ = false;

  contact_download_scheduler_->MakeImmediateRequest();
}

void NearbyShareContactManagerImpl::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  // If the allowlist changed, re-upload contacts to Nearby server.
  if (SetAllowlist(allowed_contact_ids))
    contact_upload_scheduler_->MakeImmediateRequest();
}

void NearbyShareContactManagerImpl::OnStart() {
  contact_download_scheduler_->Start();
  contact_upload_scheduler_->Start();
}

void NearbyShareContactManagerImpl::OnStop() {
  contact_download_scheduler_->Stop();
  contact_upload_scheduler_->Stop();
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
      only_download_if_changed_, local_device_data_manager_->GetId(),
      kContactDownloadRpcTimeout, http_client_factory_,
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsDownloadSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsDownloadFailure,
                     base::Unretained(this)));
  contact_downloader_->Run();
}

void NearbyShareContactManagerImpl::OnContactsDownloadSuccess(
    bool did_contacts_change_since_last_upload,
    base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts) {
  contact_downloader_.reset();

  NS_LOG(VERBOSE) << __func__ << ": Nearby Share contacts download succeeded."
                  << "\n  Did contacts change since last upload? "
                  << (did_contacts_change_since_last_upload ? "Yes." : "No.")
                  << "\n  Were contacts returned? "
                  << (contacts.has_value() ? "Yes." : "No.")
                  << "\n  Number of contacts returned: "
                  << (contacts.has_value() ? contacts->size() : 0u);
  if (contacts) {
    // A complete list of contacts was returned. Do not download list again
    // until contacts change or until explicitly requested.
    only_download_if_changed_ = true;

    // Remove contacts from the allowlist that are no longer in the contact
    // list.
    bool did_allowlist_change =
        SetAllowlist(RemoveNonexistentContactsFromAllowlist(
            GetAllowedContacts(), *contacts));

    // Notify observers that the contact list was downloaded.
    NotifyContactsDownloaded(GetAllowedContacts(), *contacts);

    // Request a contacts upload if needed, or process an existing upload
    // request now that we have the access to the full contacts list.
    switch (upload_state_) {
      case UploadState::kIdle:
        if (did_contacts_change_since_last_upload || did_allowlist_change) {
          contact_upload_scheduler_->MakeImmediateRequest();
        }
        break;
      case UploadState::kWaitingForDownload:
        StartContactsUpload(did_contacts_change_since_last_upload, *contacts);
        break;
      case UploadState::kInProgress:
        // The current upload has a stale allowlist; request another upload.
        if (did_allowlist_change) {
          contact_upload_scheduler_->MakeImmediateRequest();
        }
        // NOTE: We have no way of knowing if the contact list has changed since
        // we started our current upload--something that could only happen in a
        // very narrow window of time; we only know if the list has changed
        // since the last successful upload. We do not handle this edge case,
        // instead relying on a subsequent (periodic) download to detect that
        // the list needs to be re-uploaded.
        break;
    }
  }

  contact_download_scheduler_->HandleResult(/*success=*/true);
}

void NearbyShareContactManagerImpl::OnContactsDownloadFailure() {
  contact_download_scheduler_->HandleResult(/*success=*/false);
}

void NearbyShareContactManagerImpl::OnContactsUploadRequested() {
  DCHECK_EQ(UploadState::kIdle, upload_state_);

  NS_LOG(VERBOSE) << __func__
                  << ": Nearby Share contact upload requested. Waiting to "
                  << "download full contact list.";

  // Because the user's contact list is not persisted locally, we have to
  // retrieve the full contact list ContactRecord protos from the server
  // before uploading the list of Contact protos to the server.
  upload_state_ = UploadState::kWaitingForDownload;
  DownloadContacts(/*only_download_if_changed=*/false);
}

void NearbyShareContactManagerImpl::StartContactsUpload(
    bool did_contacts_change_since_last_upload,
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  NS_LOG(VERBOSE) << __func__
                  << ": Starting contacts upload to Nearby Share server.";
  upload_state_ = UploadState::kInProgress;
  local_device_data_manager_->UploadContacts(
      ContactRecordsToContacts(GetAllowedContacts(), contacts),
      base::BindOnce(&NearbyShareContactManagerImpl::OnContactsUploadFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     did_contacts_change_since_last_upload));
}

void NearbyShareContactManagerImpl::OnContactsUploadFinished(
    bool did_contacts_change_since_last_upload,
    bool success) {
  NS_LOG(VERBOSE) << __func__ << ": Upload of contacts to Nearby Share server "
                  << (success ? "succeeded." : "failed.")
                  << " Did contacts change since last upload? "
                  << (did_contacts_change_since_last_upload ? "Yes." : "No.");
  if (success) {
    NotifyContactsUploaded(did_contacts_change_since_last_upload);
  }
  upload_state_ = UploadState::kIdle;
  contact_upload_scheduler_->HandleResult(success);
}

bool NearbyShareContactManagerImpl::SetAllowlist(
    const std::set<std::string>& new_allowlist) {
  std::set<std::string> old_allowlist = GetAllowedContacts();
  bool were_contacts_added =
      !std::includes(old_allowlist.begin(), old_allowlist.end(),
                     new_allowlist.begin(), new_allowlist.end());
  bool were_contacts_removed =
      !std::includes(new_allowlist.begin(), new_allowlist.end(),
                     old_allowlist.begin(), old_allowlist.end());

  if (!were_contacts_added && !were_contacts_removed)
    return false;

  base::Value allowlist_value(base::Value::Type::LIST);
  for (const std::string& id : new_allowlist) {
    allowlist_value.Append(id);
  }
  pref_service_->Set(prefs::kNearbySharingAllowedContactsPrefName,
                     std::move(allowlist_value));

  NotifyAllowlistChanged(were_contacts_added, were_contacts_removed);

  return true;
}
