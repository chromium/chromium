// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : nullptr;
}

}  // namespace

// static
EasyUnlockTpmKeyManagerFactory* EasyUnlockTpmKeyManagerFactory::GetInstance() {
  return base::Singleton<EasyUnlockTpmKeyManagerFactory>::get();
}

// static
EasyUnlockTpmKeyManager* EasyUnlockTpmKeyManagerFactory::Get(
    content::BrowserContext* browser_context) {
  return static_cast<EasyUnlockTpmKeyManager*>(
      EasyUnlockTpmKeyManagerFactory::GetInstance()
          ->GetServiceForBrowserContext(browser_context, true));
}

EasyUnlockTpmKeyManager* EasyUnlockTpmKeyManagerFactory::GetForAccountId(
    const AccountId& account_id) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user = user_manager->FindUser(account_id);
  if (!user)
    return nullptr;
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return nullptr;
  return EasyUnlockTpmKeyManagerFactory::Get(profile);
}

EasyUnlockTpmKeyManagerFactory::EasyUnlockTpmKeyManagerFactory()
    : ProfileKeyedServiceFactory(
          "EasyUnlockTpmKeyManager",
          ProfileSelections::BuildRedirectedInIncognito()) {}

EasyUnlockTpmKeyManagerFactory::~EasyUnlockTpmKeyManagerFactory() = default;

KeyedService* EasyUnlockTpmKeyManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  const user_manager::User* user = nullptr;
  if (ProfileHelper::IsUserProfile(profile))
    user = ProfileHelper::Get()->GetUserByProfile(profile);
  else if (!ProfileHelper::IsSigninProfile(profile))
    return nullptr;

  return new EasyUnlockTpmKeyManager(
      user ? user->GetAccountId() : EmptyAccountId(),
      user ? user->username_hash() : std::string(), GetLocalState());
}

}  // namespace ash
