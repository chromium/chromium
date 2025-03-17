// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fake_quick_pair_browser_delegate.h"

#include <cstddef>

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/repository/fast_pair/device_address_map.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace ash::quick_pair {

namespace {
FakeQuickPairBrowserDelegate* g_instance = nullptr;
}

FakeQuickPairBrowserDelegate::FakeQuickPairBrowserDelegate() {
  SetInstanceForTesting(this);
  g_instance = this;
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
  return Shell::Get()->session_controller()->GetActivePrefService();
}

void FakeQuickPairBrowserDelegate::RequestService(
    mojo::PendingReceiver<mojom::QuickPairService> receiver) {}

bool FakeQuickPairBrowserDelegate::CompanionAppInstalled(
    const std::string& app_id) {
  if (auto search = companion_app_installed_.find(app_id);
      search != companion_app_installed_.end()) {
    return search->second;
  }
  return false;
}

void FakeQuickPairBrowserDelegate::LaunchCompanionApp(
    const std::string& app_id) {
  // Left unimplemented.
}

void FakeQuickPairBrowserDelegate::OpenPlayStorePage(GURL play_store_uri) {
  // Left unimplemented.
}

void FakeQuickPairBrowserDelegate::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void FakeQuickPairBrowserDelegate::SetCompanionAppInstalled(
    const std::string& app_id,
    bool installed) {
  companion_app_installed_[app_id] = installed;
}

}  // namespace ash::quick_pair
