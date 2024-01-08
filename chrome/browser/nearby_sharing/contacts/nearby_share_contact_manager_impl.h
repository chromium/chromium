// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

class NearbyShareClientFactory;
class NearbyShareContactDownloader;
class NearbyShareLocalDeviceDataManager;
class NearbyShareProfileInfoProvider;
class PrefService;

namespace ash::nearby {
class NearbyScheduler;
}  // namespace ash::nearby

// Implementation of NearbyShareContactManager that persists the set of allowed
// contact IDs--for selected-contacts visiblity mode--in prefs. All other
// contact data is downloaded from People API, via the NearbyShare server, as
// needed.
//
// The Nearby Share server must be explicitly informed of all contacts this
// device is aware of--needed for all-contacts visibility mode--as well as what
// contacts are allowed for selected-contacts visibility mode. These uploaded
// contact lists are used by the server to distribute the device's public
// certificates accordingly. This implementation persists a hash of the last
// uploaded contact data, and after every contacts download, a subsequent upload
// request is made if we detect that the contact list or allowlist has changed
// since the last successful upload. We also schedule periodic contact uploads
// just in case the server removed the record.
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
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareProfileInfoProvider* profile_info_provider);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareContactManager> CreateInstance(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareProfileInfoProvider* profile_info_provider) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareContactManagerImpl() override;

 private:
  NearbyShareContactManagerImpl(
      PrefService* pref_service,
      NearbyShareClientFactory* http_client_factory,
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      NearbyShareProfileInfoProvider* profile_info_provider);

  // NearbyShareContactsManager:
  void DownloadContacts() override;
  void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) override;
  std::set<std::string> GetAllowedContacts() const override;

  void OnStart() override;
  void OnStop() override;
  void Bind(mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver)
      override;

  // nearby_share::mojom::ContactsManager:
  void AddDownloadContactsObserver(
      ::mojo::PendingRemote<nearby_share::mojom::DownloadContactsObserver>
          observer) override;

  void OnPeriodicContactsUploadRequested();
  void OnContactsDownloadRequested();
  void OnContactsDownloadSuccess(
      std::vector<nearby::sharing::proto::ContactRecord> contacts,
      uint32_t num_unreachable_contacts_filtered_out);
  void OnContactsDownloadFailure();
  void OnContactsUploadFinished(bool did_contacts_change_since_last_upload,
                                const std::string& contact_upload_hash,
                                bool success);
  bool SetAllowlist(const std::set<std::string>& new_allowlist);

  // Notify the base-class and mojo observers that contacts were downloaded.
  void NotifyAllObserversContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out);

  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<NearbyShareClientFactory> http_client_factory_ = nullptr;
  raw_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_ =
      nullptr;
  raw_ptr<NearbyShareProfileInfoProvider> profile_info_provider_ = nullptr;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      periodic_contact_upload_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      contact_download_and_upload_scheduler_;
  std::unique_ptr<NearbyShareContactDownloader> contact_downloader_;
  mojo::RemoteSet<nearby_share::mojom::DownloadContactsObserver> observers_set_;
  mojo::ReceiverSet<nearby_share::mojom::ContactManager> receiver_set_;
  base::WeakPtrFactory<NearbyShareContactManagerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
