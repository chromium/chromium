// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/account_extension_tracker.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/sync_helper.h"
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
#include "extensions/browser/extension_registrar.h"
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
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

AccountExtensionTrackerFactory::AccountExtensionTrackerFactory()
    : ProfileKeyedServiceFactory(
          "AccountExtensionTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
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

std::unique_ptr<KeyedService>
AccountExtensionTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AccountExtensionTracker>(
      Profile::FromBrowserContext(context));
}

bool AccountExtensionTrackerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace

AccountExtensionTracker::~AccountExtensionTracker() = default;

AccountExtensionTracker::AccountExtensionTracker(Profile* profile)
    : profile_(profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
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

void AccountExtensionTracker::SetAccountExtensionTypeOnExtensionInstalled(
    const Extension& extension) {
  bool extension_sync_enabled = sync_util::IsSyncingExtensionsEnabled(profile_);
  bool is_syncable_extension = sync_util::ShouldSync(profile_, &extension);

  // Set to `kAccountInstalledSignedIn` if this is a syncable extension (by
  // ExtensionSyncService) that was installed when a user is signed in and has
  // sync enabled. Otherwise, set to `kLocal`.
  // Note: Treat syncable component extensions as "local" extensions despite
  // being syncable since they shouldn't be directly associated with users'
  // accounts (and they're component extensions).
  AccountExtensionType type =
      (is_syncable_extension && extension_sync_enabled &&
       !sync_helper::IsSyncableComponentExtension(&extension))
          ? AccountExtensionType::kAccountInstalledSignedIn
          : AccountExtensionType::kLocal;
  SetAccountExtensionType(extension.id(), type);
}

void AccountExtensionTracker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);

  bool observers_notified = false;

  // First look for whether or not the user has signed in or signed out.
  auto signin_event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);
  switch (signin_event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // When the user has finished the signin flow initiated from an extension
      // promo, promote all syncable extensions installed within the delay to
      // account extensions.
      for (const auto& extension_from_promo :
           extensions_installed_with_signin_promo_) {
        const Extension* extension =
            extension_registry->GetInstalledExtension(extension_from_promo);
        if (!extension) {
          continue;
        }

        DCHECK(sync_util::ShouldSync(profile_, extension));
        SetAccountExtensionType(
            extension_from_promo,
            AccountExtensionType::kAccountInstalledSignedIn);
      }

      extensions_installed_with_signin_promo_.clear();

      // Don't notify observers that extensions uploadability has changed here,
      // since initial sync data has not been received yet. Notifying here now
      // may cause UI flickers from events fired in rapid succession.
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      const ExtensionSet extensions =
          extension_registry->GenerateInstalledExtensionsSet();

      // Uninstall any signed in account extensions if
      // `uninstall_account_extensions_on_signout_` is true. Otherwise, set all
      // extensions' AccountExtensionType to `kLocal`.
      for (const auto& extension : extensions) {
        if (uninstall_account_extensions_on_signout_ &&
            GetAccountExtensionType(extension->id()) ==
                AccountExtensionType::kAccountInstalledSignedIn) {
          ExtensionRegistrar::Get(profile_)->UninstallExtension(
              extension->id(), UNINSTALL_REASON_USER_INITIATED,
              /*error=*/nullptr);
        } else {
          SetAccountExtensionType(extension->id(),
                                  AccountExtensionType::kLocal);
        }
      }

      // Reset the value of `uninstall_account_extensions_on_signout_`.
      uninstall_account_extensions_on_signout_ = false;

      NotifyOnExtensionsUploadabilityChanged();
      observers_notified = true;
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }

  // Now see if there is any change on whether the user has enabled or disabled
  // full sync. If there is, notify observers that the eligibility for uploading
  // extensions may have changed. If observers have already been notified, just
  // return early.
  if (observers_notified) {
    return;
  }

  auto sync_event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSync);
  switch (sync_event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      NotifyOnExtensionsUploadabilityChanged();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void AccountExtensionTracker::OnExtensionSyncDataReceived(
    const ExtensionId& extension_id) {
  // Only change from `kLocal` to `kAccountInstalledLocally` since the existence
  // of sync data for this extension implies it's associated with a signed in
  // user.
  // Extensions installed after sign in already have `kAccountInstalledSignedIn`
  // and thus don't need to be set here.
  AccountExtensionType type = GetAccountExtensionType(extension_id);
  if (type == AccountExtensionType::kLocal) {
    PromoteLocalToAccountExtension(
        extension_id, AccountExtensionType::kAccountInstalledLocally);
  }
}

std::vector<const Extension*>
AccountExtensionTracker::GetSignedInAccountExtensions() const {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  const ExtensionSet extensions =
      extension_registry->GenerateInstalledExtensionsSet();

  std::vector<const Extension*> signed_in_account_extensions;
  for (const auto& extension : extensions) {
    if (GetAccountExtensionType(extension->id()) ==
        AccountExtensionType::kAccountInstalledSignedIn) {
      signed_in_account_extensions.push_back(extension.get());
    }
  }

  return signed_in_account_extensions;
}

void AccountExtensionTracker::OnInitialExtensionsSyncDataReceived() {
  NotifyOnExtensionsUploadabilityChanged();
}

AccountExtensionTracker::AccountExtensionType
AccountExtensionTracker::GetAccountExtensionType(
    const ExtensionId& extension_id) const {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
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

void AccountExtensionTracker::OnSignInInitiatedFromExtensionPromo(
    const ExtensionId& extension_id) {
  bool already_has_primary_account =
      IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin);

  // Promote the extension right away if the browser already detects a signed in
  // user by the time this function is called. This happens if clicking "sign
  // in" from the promo immediately signs the user in rather than opening a
  // signin tab.
  if (already_has_primary_account) {
    PromoteLocalToAccountExtension(
        extension_id, AccountExtensionType::kAccountInstalledSignedIn);
    return;
  }

  extensions_installed_with_signin_promo_.push_back(extension_id);
  // Schedule a task to remove the `extension_id` from
  // `extensions_installed_with_signin_promo_` after
  // `kMaxSigninFromExtensionBubbleDelay`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccountExtensionTracker::RemoveExpiredExtension,
                     weak_factory_.GetWeakPtr(), extension_id),
      kMaxSigninFromExtensionBubbleDelay);
}

bool AccountExtensionTracker::CanUploadAsAccountExtension(
    const Extension& extension) const {
  // Uploading extensions as "account extensions" aka extensions syncing to the
  // current signed in user, is only enabled if the user is signed in and
  // syncing extensions in transport mode.
  // If the user is not signed in, then nothing syncs.
  // If the user is signed into full sync, then any syncable extensions
  // automatically get uploaded so user uploading is disabled.
  if (!sync_util::IsSyncingExtensionsInTransportMode(profile_)) {
    return false;
  }

  // An extension is eligible to be uploaded if it's syncable and is a local
  // extension (i.e. it's not currently syncing).
  return GetAccountExtensionType(extension.id()) ==
             AccountExtensionType::kLocal &&
         sync_util::ShouldSync(profile_, &extension);
}

void AccountExtensionTracker::OnAccountUploadInitiatedForExtension(
    const ExtensionId& extension_id) {
  // Upload the extension and change its "owner" from the current device to the
  // user's account.
  PromoteLocalToAccountExtension(
      extension_id, AccountExtensionType::kAccountInstalledSignedIn);

  // The "local state" of the uploaded extension will be pushed to sync soon,
  // so set NeedsSync to false.
  ExtensionPrefs::Get(profile_)->SetNeedsSync(extension_id, false);
}

void AccountExtensionTracker::SetAccountExtensionTypeForTesting(
    const ExtensionId& extension_id,
    AccountExtensionType type) {
  SetAccountExtensionType(extension_id, type);
}

void AccountExtensionTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccountExtensionTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AccountExtensionTracker::SetAccountExtensionType(
    const ExtensionId& extension_id,
    AccountExtensionTracker::AccountExtensionType type) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  prefs->SetIntegerPref(extension_id, kAccountExtensionTypePref, type);
}

void AccountExtensionTracker::RemoveExpiredExtension(
    const ExtensionId& extension_id) {
  std::erase_if(
      extensions_installed_with_signin_promo_,
      [&extension_id](const ExtensionId& id) { return extension_id == id; });
}

void AccountExtensionTracker::PromoteLocalToAccountExtension(
    const ExtensionId& extension_id,
    AccountExtensionType type) {
  // Make sure we're actually promoting a local extension to an account
  // extension!
  DCHECK_EQ(GetAccountExtensionType(extension_id),
            AccountExtensionType::kLocal);
  DCHECK_NE(type, AccountExtensionType::kLocal);
  SetAccountExtensionType(extension_id, type);

  // The extension's uploadability may change when its AccountExtensionType
  // changes.
  for (auto& observer : observers_) {
    observer.OnExtensionUploadabilityChanged(extension_id);
  }
}

void AccountExtensionTracker::NotifyOnExtensionsUploadabilityChanged() {
  for (auto& observer : observers_) {
    observer.OnExtensionsUploadabilityChanged();
  }
}

}  // namespace extensions
