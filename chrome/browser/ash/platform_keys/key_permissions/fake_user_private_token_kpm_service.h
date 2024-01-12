// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_FAKE_USER_PRIVATE_TOKEN_KPM_SERVICE_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_FAKE_USER_PRIVATE_TOKEN_KPM_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace ash {
namespace platform_keys {

class KeyPermissionsManager;

// A fake UserPrivateTokenKeyPermissionsManagerService which returns a
// KeyPermissionsManager pointer passed to its constructor.
class FakeUserPrivateTokenKeyPermissionsManagerService
    : public UserPrivateTokenKeyPermissionsManagerService {
 public:
  explicit FakeUserPrivateTokenKeyPermissionsManagerService(
      platform_keys::KeyPermissionsManager* key_permissions_manager);
  FakeUserPrivateTokenKeyPermissionsManagerService(
      const FakeUserPrivateTokenKeyPermissionsManagerService&) = delete;
  FakeUserPrivateTokenKeyPermissionsManagerService& operator=(
      const FakeUserPrivateTokenKeyPermissionsManagerService&) = delete;
  ~FakeUserPrivateTokenKeyPermissionsManagerService() override;

  platform_keys::KeyPermissionsManager* key_permissions_manager() override;
  void Shutdown() override;

 private:
  raw_ptr<platform_keys::KeyPermissionsManager, DanglingUntriaged>
      key_permissions_manager_ = nullptr;
};

std::unique_ptr<KeyedService>
BuildFakeUserPrivateTokenKeyPermissionsManagerService(
    platform_keys::KeyPermissionsManager* key_permissions_manager,
    content::BrowserContext* browser_context);

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_FAKE_USER_PRIVATE_TOKEN_KPM_SERVICE_H_
