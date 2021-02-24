// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace {

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : NULL;
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

EasyUnlockTpmKeyManager* EasyUnlockTpmKeyManagerFactory::GetForUser(
    const std::string& user_id) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      user_manager->FindUser(user_manager::known_user::GetAccountId(
          user_id, std::string() /* id */, AccountType::UNKNOWN));
  if (!user)
    return NULL;
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return NULL;
  return EasyUnlockTpmKeyManagerFactory::Get(profile);
}

EasyUnlockTpmKeyManagerFactory::EasyUnlockTpmKeyManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "EasyUnlockTpmKeyManager",
          BrowserContextDependencyManager::GetInstance()) {}

EasyUnlockTpmKeyManagerFactory::~EasyUnlockTpmKeyManagerFactory() {}

KeyedService* EasyUnlockTpmKeyManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  const user_manager::User* user = NULL;
  if (ProfileHelper::IsRegularProfile(profile))
    user = ProfileHelper::Get()->GetUserByProfile(profile);
  else if (!ProfileHelper::IsSigninProfile(profile))
    return nullptr;

  return new EasyUnlockTpmKeyManager(
      user ? user->GetAccountId() : EmptyAccountId(),
      user ? user->username_hash() : std::string(), GetLocalState());
}

content::BrowserContext* EasyUnlockTpmKeyManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace chromeos
