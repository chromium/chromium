// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"
#include <set>
#include <string>

FakeNearbyShareContactManager::Factory::Factory() = default;

FakeNearbyShareContactManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareContactManager>
FakeNearbyShareContactManager::Factory::CreateInstance(
    PrefService* pref_service,
    NearbyShareClientFactory* http_client_factory,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  latest_pref_service_ = pref_service;
  latest_http_client_factory_ = http_client_factory;
  latest_local_device_data_manager_ = local_device_data_manager;
  latest_profile_info_provider_ = profile_info_provider;

  auto instance = std::make_unique<FakeNearbyShareContactManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareContactManager::FakeNearbyShareContactManager() = default;

FakeNearbyShareContactManager::~FakeNearbyShareContactManager() = default;

void FakeNearbyShareContactManager::DownloadContacts() {
  ++num_download_contacts_calls_;
}

void FakeNearbyShareContactManager::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  set_allowed_contacts_calls_.push_back(allowed_contact_ids);
}
std::set<std::string> FakeNearbyShareContactManager::GetAllowedContacts()
    const {
  return set_allowed_contacts_calls_.empty()
             ? std::set<std::string>()
             : set_allowed_contacts_calls_.back();
}

void FakeNearbyShareContactManager::OnStart() {}

void FakeNearbyShareContactManager::OnStop() {}

void FakeNearbyShareContactManager::Bind(
    mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) {}

void FakeNearbyShareContactManager::AddDownloadContactsObserver(
    ::mojo::PendingRemote<nearby_share::mojom::DownloadContactsObserver>
        observer) {}
