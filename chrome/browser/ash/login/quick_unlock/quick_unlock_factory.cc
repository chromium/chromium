// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace quick_unlock {

// static
QuickUnlockStorage* QuickUnlockFactory::GetForProfile(Profile* profile) {
  return static_cast<QuickUnlockStorage*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
QuickUnlockStorage* QuickUnlockFactory::GetForUser(
    const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return nullptr;

  return GetForProfile(profile);
}

// static
QuickUnlockStorage* QuickUnlockFactory::GetForAccountId(
    const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user)
    return nullptr;

  return GetForUser(user);
}

// static
QuickUnlockFactory* QuickUnlockFactory::GetInstance() {
  static base::NoDestructor<QuickUnlockFactory> instance;
  return instance.get();
}

ash::auth::QuickUnlockStorageDelegate& QuickUnlockFactory::GetDelegate() {
  class Delegate : public auth::QuickUnlockStorageDelegate {
    UserContext* GetUserContext(const ::user_manager::User* user,
                                const std::string& token) override {
      if (!user) {
        LOG(ERROR) << "Invalid user";
        return nullptr;
      }
      QuickUnlockStorage* storage = GetForUser(user);
      if (!storage) {
        LOG(ERROR) << "User does not have a QuickUnlockStorage";
        return nullptr;
      }
      return storage->GetUserContext(token);
    }

    void SetUserContext(const ::user_manager::User* user,
                        std::unique_ptr<UserContext> context) override {
      if (!user) {
        LOG(ERROR) << "Invalid user";
        return;
      }
      QuickUnlockStorage* storage =
          quick_unlock::QuickUnlockFactory::GetForUser(user);
      if (!user) {
        LOG(ERROR) << "User does not have a QuickUnlockStorage";
        return;
      }
      quick_unlock::AuthToken* auth_token = storage->GetAuthToken();
      if (auth_token == nullptr || auth_token->user_context() == nullptr) {
        // If this happens, it means that the auth token expired. In this case,
        // the user needs to reauthenticate, and a new context will be created.
        return;
      }

      auth_token->ReplaceUserContext(std::move(context));
    }

    PrefService* GetPrefService(const ::user_manager::User& user) override {
      Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(&user);
      CHECK(profile);
      return profile->GetPrefs();
    }
  };

  static base::NoDestructor<Delegate> delegate;
  return *delegate;
}

QuickUnlockFactory::QuickUnlockFactory()
    : ProfileKeyedServiceFactory(
          "QuickUnlockFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

QuickUnlockFactory::~QuickUnlockFactory() = default;

std::unique_ptr<KeyedService>
QuickUnlockFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<QuickUnlockStorage>(
      Profile::FromBrowserContext(context));
}

}  // namespace quick_unlock
}  // namespace ash
