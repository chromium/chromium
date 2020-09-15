// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class NearbyShareClientFactory;
class NearbyShareContactDownloader;
class NearbyShareLocalDeviceDataManager;
class NearbyShareScheduler;
class PrefService;

// Implementation of NearbyShareContactManager that persists the set of allowed
// contact IDs--for selected-contacts visiblity mode--in prefs. All other
// contact data is downloaded from People API, via the NearbyShare server, as
// needed.
//
// The Nearby Share server must be explicitly informed of all contacts this
// device is aware of--needed for all-contacts visibility mode--as well as what
// contacts are allowed for selected-contacts visibility mode. The
// NearbyShareContactManagerImpl controls when contacts are uploaded to the
// server: 1) when the server communicates that the contact list has changed
// since the last upload, or 2) when the user locally makes changes to the list
// of selected contacts. These uploaded contact lists are used by the server to
// distribute the device's public certificates accordingly.
//
// In addition to supporting on-demand contact downloads, this implementation
// periodically checks in with the Nearby Share server to see if the user's
// contact list has changed since the last upload.
class NearbyShareContactManagerImpl : public NearbyShareContactManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareContactManager> Create(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareLocalDeviceDataManager* local_device_data_manager);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareContactManager> CreateInstance(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareLocalDeviceDataManager* local_device_data_manager) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareContactManagerImpl() override;

 private:
  enum class UploadState { kIdle, kWaitingForDownload, kInProgress };

  NearbyShareContactManagerImpl(
      PrefService* pref_service,
      NearbyShareClientFactory* http_client_factory,
      NearbyShareLocalDeviceDataManager* local_device_data_manager);

  // NearbyShareContactsManager:
  void DownloadContacts(bool only_download_if_changed) override;
  void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) override;
  void OnStart() override;
  void OnStop() override;
  void Bind(mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver)
      override;

  void NotifyMojoObserverContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts);

  // nearby_share::mojom::ContactsManager:
  void AddDownloadContactsObserver(
      ::mojo::PendingRemote<nearby_share::mojom::DownloadContactsObserver>
          observer) override;
  void DownloadContacts() override;

  std::set<std::string> GetAllowedContacts() const;
  void OnContactsDownloadRequested();
  void OnContactsDownloadSuccess(
      bool did_contacts_change_since_last_upload,
      base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts);
  void OnContactsDownloadFailure();
  void OnContactsUploadRequested();
  void StartContactsUpload(
      bool did_contacts_change_since_last_upload,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts);
  void OnContactsUploadFinished(bool did_contacts_change_since_last_upload,
                                bool success);
  bool SetAllowlist(const std::set<std::string>& new_allowlist);

  // By default, only download contacts if they have changed since the last
  // upload. Only set to false on explicit request from DownloadContacts(), and
  // reset to true after a successful contact download.
  bool only_download_if_changed_ = true;

  UploadState upload_state_ = UploadState::kIdle;
  PrefService* pref_service_ = nullptr;
  NearbyShareClientFactory* http_client_factory_ = nullptr;
  NearbyShareLocalDeviceDataManager* local_device_data_manager_ = nullptr;
  std::unique_ptr<NearbyShareScheduler> contact_download_scheduler_;
  std::unique_ptr<NearbyShareScheduler> contact_upload_scheduler_;
  std::unique_ptr<NearbyShareContactDownloader> contact_downloader_;
  mojo::RemoteSet<nearby_share::mojom::DownloadContactsObserver> observers_set_;
  mojo::ReceiverSet<nearby_share::mojom::ContactManager> receiver_set_;
  base::WeakPtrFactory<NearbyShareContactManagerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
