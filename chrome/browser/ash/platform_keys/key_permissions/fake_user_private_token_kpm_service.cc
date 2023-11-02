// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/fake_user_private_token_kpm_service.h"

#include <memory>

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace platform_keys {

FakeUserPrivateTokenKeyPermissionsManagerService::
    FakeUserPrivateTokenKeyPermissionsManagerService(
        platform_keys::KeyPermissionsManager* key_permissions_manager)
    : UserPrivateTokenKeyPermissionsManagerService() {
  key_permissions_manager_ = key_permissions_manager;
}

FakeUserPrivateTokenKeyPermissionsManagerService::
    ~FakeUserPrivateTokenKeyPermissionsManagerService() = default;

platform_keys::KeyPermissionsManager*
FakeUserPrivateTokenKeyPermissionsManagerService::key_permissions_manager() {
  return key_permissions_manager_;
}

void FakeUserPrivateTokenKeyPermissionsManagerService::Shutdown() {}

std::unique_ptr<KeyedService>
BuildFakeUserPrivateTokenKeyPermissionsManagerService(
    platform_keys::KeyPermissionsManager* key_permissions_manager,
    content::BrowserContext* browser_context) {
  return std::make_unique<FakeUserPrivateTokenKeyPermissionsManagerService>(
      key_permissions_manager);
}

}  // namespace platform_keys
}  // namespace ash
