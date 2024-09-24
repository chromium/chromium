// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/account_extension_tracker.h"

#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

namespace {

constexpr PrefMap kAccountExtensionTypePref = {"account_extension_type",
                                               PrefType::kInteger,
                                               PrefScope::kExtensionSpecific};

class AccountExtensionTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  AccountExtensionTrackerFactory();
  AccountExtensionTrackerFactory(const AccountExtensionTrackerFactory&) =
      delete;
  AccountExtensionTrackerFactory& operator=(
      const AccountExtensionTrackerFactory&) = delete;
  ~AccountExtensionTrackerFactory() override = default;

  AccountExtensionTracker* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

AccountExtensionTrackerFactory::AccountExtensionTrackerFactory()
    : ProfileKeyedServiceFactory(
          "AccountExtensionTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountExtensionTracker* AccountExtensionTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<AccountExtensionTracker*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

KeyedService* AccountExtensionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccountExtensionTracker(context);
}

bool AccountExtensionTrackerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace

AccountExtensionTracker::~AccountExtensionTracker() = default;

AccountExtensionTracker::AccountExtensionTracker(
    content::BrowserContext* context)
    : browser_context_(context) {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(context);
  extension_registry_observation_.Observe(extension_registry);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  identity_manager_observation_.Observe(identity_manager);
}

// static
AccountExtensionTracker* AccountExtensionTracker::Get(
    content::BrowserContext* context) {
  return static_cast<AccountExtensionTrackerFactory*>(GetFactory())
      ->GetForBrowserContext(context);
}

// static
BrowserContextKeyedServiceFactory* AccountExtensionTracker::GetFactory() {
  static base::NoDestructor<AccountExtensionTrackerFactory> g_factory;
  return g_factory.get();
}

void AccountExtensionTracker::OnExtensionInstalled(
    content::BrowserContext* context,
    const Extension* extension,
    bool is_update) {
  // Ignore updates since `OnExtensionSyncDataApplied` should handle incoming
  // sync data, and these may not trigger updates based on the extension's
  // version vs the version in the sync data.
  if (is_update) {
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(Profile::FromBrowserContext(context));
  bool extension_sync_enabled =
      sync_service && sync_service->GetUserSettings()->GetSelectedTypes().Has(
                          syncer::UserSelectableType::kExtensions);
  bool is_syncable_extension =
      ExtensionSyncService::ShouldSync(context, *extension);

  // Set to `kAccountInstalledSignedIn` if this is a syncable extension (by
  // ExtensionSyncService) that was installed when a user is signed in and has
  // sync enabled. Otherwise, set to `kLocal`.
  AccountExtensionType type =
      (is_syncable_extension && extension_sync_enabled)
          ? AccountExtensionType::kAccountInstalledSignedIn
          : AccountExtensionType::kLocal;
  SetAccountExtensionType(extension->id(), type);
}

void AccountExtensionTracker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // TODO(crbug.com/366474682): If extension syncing is enabled in transport
  // mode, only set the pref if the user chooses to keep extensions when signing
  // out.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    ExtensionRegistry* extension_registry =
        ExtensionRegistry::Get(browser_context_);
    const ExtensionSet extensions =
        extension_registry->GenerateInstalledExtensionsSet();

    for (const auto& extension : extensions) {
      SetAccountExtensionType(extension->id(), AccountExtensionType::kLocal);
    }
  }
}

void AccountExtensionTracker::OnExtensionSyncDataApplied(
    const ExtensionId& extension_id) {
  // Only change from `kLocal` to `kAccountInstalledLocally` since the existence
  // of sync data for this extension implies it's associated with a signed in
  // user.
  // Extensions installed after sign in already have `kAccountInstalledSignedIn`
  // and thus don't need to be set here.
  AccountExtensionType type = GetAccountExtensionType(extension_id);
  if (type == AccountExtensionType::kLocal) {
    SetAccountExtensionType(extension_id,
                            AccountExtensionType::kAccountInstalledLocally);
  }
}

AccountExtensionTracker::AccountExtensionType
AccountExtensionTracker::GetAccountExtensionTypeForTesting(
    const ExtensionId& extension_id) const {
  return GetAccountExtensionType(extension_id);
}

AccountExtensionTracker::AccountExtensionType
AccountExtensionTracker::GetAccountExtensionType(
    const ExtensionId& extension_id) const {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  int type_int = 0;

  // If the pref does not exist or is corrupted (not a valid value), return
  // `kLocal` as a fallback.
  if (!prefs->ReadPrefAsInteger(extension_id, kAccountExtensionTypePref,
                                &type_int)) {
    return AccountExtensionType::kLocal;
  }

  if (type_int < 0 || type_int > AccountExtensionType::kLast) {
    return AccountExtensionType::kLocal;
  }

  return static_cast<AccountExtensionType>(type_int);
}

void AccountExtensionTracker::SetAccountExtensionType(
    const ExtensionId& extension_id,
    AccountExtensionTracker::AccountExtensionType type) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  prefs->SetIntegerPref(extension_id, kAccountExtensionTypePref, type);
}

}  // namespace extensions
