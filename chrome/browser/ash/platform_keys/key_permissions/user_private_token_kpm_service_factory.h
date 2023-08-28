// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_USER_PRIVATE_TOKEN_KPM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_USER_PRIVATE_TOKEN_KPM_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ash {
namespace platform_keys {

// This is a service which holds the user-specific KeyPermissionsManager for a
// Profile.
class UserPrivateTokenKeyPermissionsManagerService : public KeyedService {
 public:
  explicit UserPrivateTokenKeyPermissionsManagerService(Profile* profile);
  UserPrivateTokenKeyPermissionsManagerService(
      const UserPrivateTokenKeyPermissionsManagerService&) = delete;
  UserPrivateTokenKeyPermissionsManagerService& operator=(
      const UserPrivateTokenKeyPermissionsManagerService&) = delete;
  ~UserPrivateTokenKeyPermissionsManagerService() override;

  // KeyedService
  void Shutdown() override;

  virtual KeyPermissionsManager* key_permissions_manager();

 protected:
  // Used by FakeUserPrivateTokenKeyPermissionsManagerService
  UserPrivateTokenKeyPermissionsManagerService();

 private:
  std::unique_ptr<KeyPermissionsManager> key_permissions_manager_;
};

class UserPrivateTokenKeyPermissionsManagerServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static UserPrivateTokenKeyPermissionsManagerService* GetForBrowserContext(
      content::BrowserContext* context);

  static UserPrivateTokenKeyPermissionsManagerServiceFactory* GetInstance();

  UserPrivateTokenKeyPermissionsManagerServiceFactory();
  UserPrivateTokenKeyPermissionsManagerServiceFactory(
      const UserPrivateTokenKeyPermissionsManagerServiceFactory&) = delete;
  UserPrivateTokenKeyPermissionsManagerServiceFactory& operator=(
      const UserPrivateTokenKeyPermissionsManagerServiceFactory&) = delete;
  ~UserPrivateTokenKeyPermissionsManagerServiceFactory() override;

 private:
  friend base::NoDestructor<
      UserPrivateTokenKeyPermissionsManagerServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_USER_PRIVATE_TOKEN_KPM_SERVICE_FACTORY_H_
