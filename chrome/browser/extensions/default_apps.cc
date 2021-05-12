// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/external_web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// Returns true if the app was a default app in Chrome 22
bool IsOldDefaultApp(const std::string& extension_id) {
  return extension_id == extension_misc::kGmailAppId ||
         extension_id == extension_misc::kYoutubeAppId;
}

bool IsLocaleSupported() {
  // Don't bother installing default apps in locales where it is known that
  // they don't work.
  // TODO(rogerta): Do this check dynamically once the webstore can expose
  // an API. See http://crbug.com/101357
  const std::string& locale = g_browser_process->GetApplicationLocale();
  static const char* const unsupported_locales[] = {"CN", "TR", "IR"};
  for (size_t i = 0; i < base::size(unsupported_locales); ++i) {
    if (base::EndsWith(locale, unsupported_locales[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }
  return true;
}

base::LazyInstance<std::set<Profile*>>::Leaky g_perform_new_installation =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace default_apps {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown);
}

// static
bool Provider::DidPerformNewInstallationForProfile(Profile* profile) {
  return g_perform_new_installation.Get().count(profile);
}

void Provider::InitProfileState() {
  // We decide to install or not install default apps based on the following
  // criteria, from highest priority to lowest priority:
  //
  // - If the locale is not compatible with the defaults, don't install them.
  // - The kDefaultApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  // - If they have already been installed, don't reinstall them.

  default_apps_enabled_ =
      IsLocaleSupported() &&
      profile_->GetPrefs()->GetString(prefs::kDefaultApps) == "install";
  DCHECK(!perform_new_installation_);

  InstallState state =
      static_cast<InstallState>(profile_->GetPrefs()->GetInteger(
          prefs::kDefaultAppsInstallState));

  base::Optional<InstallState> new_install_state;

  switch (state) {
    case kUnknown: {
      // Default apps are only installed on profile creation or a new chrome
      // download.
      bool is_new_profile = profile_->WasCreatedByVersionOrLater(
          version_info::GetVersionNumber());
      if (is_new_profile && default_apps_enabled_) {
        new_install_state = kAlreadyInstalledDefaultApps;
        perform_new_installation_ = true;
      } else {
        new_install_state = kNeverInstallDefaultApps;
      }
      break;
    }

    // The old default apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old default apps to new mechanism where
    // they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case kProvideLegacyDefaultApps:
      is_migration_ = true;
      new_install_state = kAlreadyInstalledDefaultApps;
      break;

    case kAlreadyInstalledDefaultApps:
    case kNeverInstallDefaultApps:
      break;

    default:
      NOTREACHED();
  }

  if (new_install_state) {
    profile_->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                     *new_install_state);
  }
  if (perform_new_installation_)
    g_perform_new_installation.Get().insert(profile_);
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   scoped_refptr<extensions::ExternalLoader> loader,
                   extensions::mojom::ManifestLocation crx_location,
                   extensions::mojom::ManifestLocation download_location,
                   int creation_flags)
    : extensions::ExternalProviderImpl(service,
                                       std::move(loader),
                                       profile,
                                       crx_location,
                                       download_location,
                                       creation_flags),
      profile_(profile) {
  DCHECK(profile);
  set_auto_acknowledge(true);

  InitProfileState();
}

void Provider::VisitRegisteredExtension() {
  if (!default_apps_enabled_) {
    // If default apps aren't enabled for the profile, we short-circuit the
    // flow to load them from the file (which happens as a result of
    // VisitRegisteredExtension()), and immediately set empty prefs.
    ExternalProviderImpl::SetPrefs(std::make_unique<base::DictionaryValue>());
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

void Provider::SetPrefs(std::unique_ptr<base::DictionaryValue> prefs) {
  DCHECK(default_apps_enabled_);

  // First, check if this is for a migration from around 2013. Likely not.
  if (is_migration_) {
    DCHECK(!perform_new_installation_);
    std::set<std::string> keys_to_erase;
    // Filter out the new default apps for migrating users, so that we don't
    // randomly install them out of the blue.
    // Two-pass to keep iterators nice and happy.
    for (const auto& entry : prefs->DictItems()) {
      if (!IsOldDefaultApp(entry.first))
        keys_to_erase.insert(entry.first);
    }
    for (const auto& key : keys_to_erase)
      prefs->Remove(key, nullptr);
  }

  // Next, the more fun case. It's possible that these apps were uninstalled
  // as part of the web app migration. But, the web app migration could have
  // been rolled back. If that happened, we need to reinstall the extension
  // apps.
  if (!perform_new_installation_) {
    auto should_re_add_app = [profile = profile_](const std::string& id,
                                                  const base::Value& pref) {
      if (!pref.is_dict())
        return false;  // Invalid entry; it'll be ignored later.
      const std::string* web_app_flag =
          pref.FindStringPath(kWebAppMigrationFlag);
      if (!web_app_flag)
        return false;  // Isn't migrating.
      if (web_app::IsExternalAppInstallFeatureEnabled(*web_app_flag,
                                                      *profile)) {
        // The feature is still enabled; it's responsible for the behavior.
        return false;
      }
      if (!web_app::WasAppMigratedToWebApp(profile, id)) {
        // The web app was not previously migrated to a web app; don't do
        // anything special for it.
        return false;
      }

      // The edge case! We found an app that was migrated to a web app, but now
      // the feature is disabled. We need to re-add it.
      return true;
    };

    std::set<std::string> keys_to_erase;
    for (const auto& entry : prefs->DictItems()) {
      bool should_re_add = should_re_add_app(entry.first, entry.second);
      if (should_re_add) {
        // Since it will be re-added, mark it as no-longer-migrated.
        web_app::MarkAppAsMigratedToWebApp(profile_, entry.first, false);
      } else {
        keys_to_erase.insert(entry.first);
      }
    }

    for (const auto& key : keys_to_erase) {
      prefs->Remove(key, nullptr);
    }
  }

  ExternalProviderImpl::SetPrefs(std::move(prefs));
}

}  // namespace default_apps
