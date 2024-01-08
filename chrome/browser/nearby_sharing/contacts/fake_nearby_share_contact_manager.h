// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NearbyShareProfileInfoProvider;
class PrefService;

// A fake implementation of NearbyShareContactManager, along with a fake
// factory, to be used in tests. Stores parameters input into
// NearbyShareContactManager method calls. Use the notification methods from the
// base class--NotifyContactsDownloaded() and NotifyContactsUploaded()--to alert
// observers of changes; these methods are made public in this fake class.
class FakeNearbyShareContactManager : public NearbyShareContactManager {
 public:
  // Factory that creates FakeNearbyShareContactManager instances. Use in
  // NearbyShareContactManagerImpl::Factor::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareContactManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareContactManager instances created by
    // CreateInstance().
    std::vector<raw_ptr<FakeNearbyShareContactManager, VectorExperimental>>&
    instances() {
      return instances_;
    }

    PrefService* latest_pref_service() const { return latest_pref_service_; }

    NearbyShareClientFactory* latest_http_client_factory() const {
      return latest_http_client_factory_;
    }

    NearbyShareLocalDeviceDataManager* latest_local_device_data_manager()
        const {
      return latest_local_device_data_manager_;
    }

    NearbyShareProfileInfoProvider* latest_profile_info_provider() const {
      return latest_profile_info_provider_;
    }

   private:
    // NearbyShareContactManagerImpl::Factory:
    std::unique_ptr<NearbyShareContactManager> CreateInstance(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareProfileInfoProvider* profile_info_provider) override;

    std::vector<raw_ptr<FakeNearbyShareContactManager, VectorExperimental>>
        instances_;
    raw_ptr<PrefService> latest_pref_service_ = nullptr;
    raw_ptr<NearbyShareClientFactory, DanglingUntriaged>
        latest_http_client_factory_ = nullptr;
    raw_ptr<NearbyShareLocalDeviceDataManager, DanglingUntriaged>
        latest_local_device_data_manager_ = nullptr;
    raw_ptr<NearbyShareProfileInfoProvider, DanglingUntriaged>
        latest_profile_info_provider_ = nullptr;
  };

  FakeNearbyShareContactManager();
  ~FakeNearbyShareContactManager() override;

  // NearbyShareContactsManager:
  void DownloadContacts() override;
  void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) override;
  std::set<std::string> GetAllowedContacts() const override;

  size_t num_download_contacts_calls() const {
    return num_download_contacts_calls_;
  }

  // Returns inputs of all SetAllowedContacts() calls.
  const std::vector<std::set<std::string>>& set_allowed_contacts_calls() const {
    return set_allowed_contacts_calls_;
  }

  // Make protected methods from base class public in this fake class.
  using NearbyShareContactManager::NotifyContactsDownloaded;
  using NearbyShareContactManager::NotifyContactsUploaded;

 private:
  // NearbyShareContactsManager:
  void OnStart() override;
  void OnStop() override;
  void Bind(mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver)
      override;

  // nearby_share::mojom::ContactsManager:
  void AddDownloadContactsObserver(
      ::mojo::PendingRemote<nearby_share::mojom::DownloadContactsObserver>
          observer) override;

  size_t num_download_contacts_calls_ = 0;
  std::vector<std::set<std::string>> set_allowed_contacts_calls_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
