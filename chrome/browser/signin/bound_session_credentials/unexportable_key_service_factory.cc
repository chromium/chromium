// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/keyed_unexportable_key_service_impl.h"
#include "chrome/common/chrome_version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroup[] = MAC_TEAM_IDENTIFIER_STRING
    "." MAC_BUNDLE_IDENTIFIER_STRING ".unexportable-keys";
#endif  // BUILDFLAG(IS_MAC)

// Returns a newly created task manager instance, or nullptr if unexportable
// keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  crypto::UnexportableKeyProvider::Config config{
#if BUILDFLAG(IS_MAC)
      .keychain_access_group = kKeychainAccessGroup,
#endif  // BUILDFLAG(IS_MAC)
  };
  if (!unexportable_keys::UnexportableKeyServiceImpl::
          IsUnexportableKeyProviderSupported(config)) {
    return nullptr;
  }
  return std::make_unique<unexportable_keys::UnexportableKeyTaskManager>(
      std::move(config));
}

// Returns an `UnexportableKeyTaskManager` instance that is shared across all
// profiles, or nullptr if unexportable keys are not available. This function
// caches availability, so any flags that may change it must be set before the
// first call.
//
// Note: this instance is currently accessible only to
// `UnexportableKeyServiceFactory`. The getter can be moved to some common place
// if there is a need.
unexportable_keys::UnexportableKeyTaskManager* GetSharedTaskManagerInstance() {
  static base::NoDestructor<
      std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>>
      instance(CreateTaskManagerInstance());
  return instance->get();
}

}  // namespace

// static
unexportable_keys::UnexportableKeyService*
UnexportableKeyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<KeyedUnexportableKeyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
UnexportableKeyServiceFactory* UnexportableKeyServiceFactory::GetInstance() {
  static base::NoDestructor<UnexportableKeyServiceFactory> instance;
  return instance.get();
}

UnexportableKeyServiceFactory::UnexportableKeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnexportableKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Only an OTR profile is used for browsing in the Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

UnexportableKeyServiceFactory::~UnexportableKeyServiceFactory() = default;

// ProfileKeyedServiceFactory:
std::unique_ptr<KeyedService>
UnexportableKeyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Do not create a service if all consumer features are disabled.
  if (!switches::IsBoundSessionCredentialsEnabled(profile->GetPrefs()) &&
      !switches::IsChromeRefreshTokenBindingEnabled(profile->GetPrefs())) {
    return nullptr;
  }

  unexportable_keys::UnexportableKeyTaskManager* task_manager =
      GetSharedTaskManagerInstance();
  if (!task_manager) {
    // Do not create a service if the platform doesn't support unexportable
    // signing keys.
    return nullptr;
  }

  return std::make_unique<KeyedUnexportableKeyServiceImpl>(*task_manager);
}
