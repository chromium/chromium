// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fake_quick_pair_browser_delegate.h"
#include <cstddef>
#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/repository/fast_pair/device_address_map.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::quick_pair {

namespace {
FakeQuickPairBrowserDelegate* g_instance = nullptr;
}

FakeQuickPairBrowserDelegate::FakeQuickPairBrowserDelegate() {
  SetInstanceForTesting(this);
  g_instance = this;
  PendingWriteStore::RegisterProfilePrefs(pref_service_.registry());
  SavedDeviceRegistry::RegisterProfilePrefs(pref_service_.registry());
  DeviceAddressMap::RegisterLocalStatePrefs(pref_service_.registry());
  pref_service_.registry()->RegisterBooleanPref(ash::prefs::kFastPairEnabled,
                                                /*default_value=*/true);
}

FakeQuickPairBrowserDelegate::~FakeQuickPairBrowserDelegate() {
  SetInstanceForTesting(nullptr);
  g_instance = nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeQuickPairBrowserDelegate::GetURLLoaderFactory() {
  return nullptr;
}

signin::IdentityManager* FakeQuickPairBrowserDelegate::GetIdentityManager() {
  return identity_manager_;
}

std::unique_ptr<image_fetcher::ImageFetcher>
FakeQuickPairBrowserDelegate::GetImageFetcher() {
  return nullptr;
}

FakeQuickPairBrowserDelegate* FakeQuickPairBrowserDelegate::Get() {
  return g_instance;
}

PrefService* FakeQuickPairBrowserDelegate::GetActivePrefService() {
  return &pref_service_;
}

void FakeQuickPairBrowserDelegate::RequestService(
    mojo::PendingReceiver<mojom::QuickPairService> receiver) {}

void FakeQuickPairBrowserDelegate::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

}  // namespace ash::quick_pair
