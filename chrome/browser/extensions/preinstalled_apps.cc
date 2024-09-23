// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/preinstalled_apps.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

// Returns true if the app was a pre-installed app in Chrome 22
bool IsOldPreinstalledApp(const std::string& extension_id) {
  return extension_id == extension_misc::kGmailAppId ||
         extension_id == extension_misc::kYoutubeAppId;
}

bool IsLocaleSupported() {
  // Don't bother installing pre-installed apps in locales where it is known
  // that they don't work.
  // TODO(rogerta): Do this check dynamically once the webstore can expose
  // an API. See http://crbug.com/101357
  std::string locale =
      extensions::ExtensionsBrowserClient::Get()->GetApplicationLocale();
  static constexpr const char* unsupported_locales[] = {"CN", "TR", "IR"};
  for (size_t i = 0; i < std::size(unsupported_locales); ++i) {
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

namespace preinstalled_apps {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPreinstalledAppsInstallState, kUnknown);
}

// static
bool Provider::DidPerformNewInstallationForProfile(Profile* profile) {
  return g_perform_new_installation.Get().count(profile);
}

void Provider::InitProfileState() {
  // We decide to install or not install pre-installed apps based on the
  // following criteria, from highest priority to lowest priority:
  //
  // - If the locale is not compatible with the pre-installed apps, don't
  // install them.
  // - The kPreinstalledApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  // - If they have already been installed, don't reinstall them.

  preinstalled_apps_enabled_ =
      IsLocaleSupported() &&
      profile_->GetPrefs()->GetString(prefs::kPreinstalledApps) == "install";
  DCHECK(!perform_new_installation_);

  InstallState state = static_cast<InstallState>(
      profile_->GetPrefs()->GetInteger(prefs::kPreinstalledAppsInstallState));

  std::optional<InstallState> new_install_state;

  switch (state) {
    case kUnknown: {
      // Pre-installed apps are only installed on profile creation or a new
      // chrome download.
      bool is_new_profile = profile_->WasCreatedByVersionOrLater(
          std::string(version_info::GetVersionNumber()));
      if (is_new_profile && preinstalled_apps_enabled_) {
        new_install_state = kAlreadyInstalledPreinstalledApps;
        perform_new_installation_ = true;
      } else {
        new_install_state = kNeverInstallPreinstalledApps;
      }
      break;
    }

    // The old pre-installed apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old pre-installed apps to new mechanism
    // where they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case kProvideLegacyPreinstalledApps:
      is_migration_ = true;
      new_install_state = kAlreadyInstalledPreinstalledApps;
      break;

    case kAlreadyInstalledPreinstalledApps:
    case kNeverInstallPreinstalledApps:
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (new_install_state) {
    profile_->GetPrefs()->SetInteger(prefs::kPreinstalledAppsInstallState,
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
  if (!preinstalled_apps_enabled_) {
    // If pre-installed apps aren't enabled for the profile, we short-circuit
    // the flow to load them from the file (which happens as a result of
    // VisitRegisteredExtension()), and immediately set empty prefs.
    ExternalProviderImpl::SetPrefs(base::Value::Dict());
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

void Provider::SetPrefs(base::Value::Dict prefs) {
  DCHECK(preinstalled_apps_enabled_);

  // First, check if this is for a migration from around 2013. Likely not.
  if (is_migration_) {
    DCHECK(!perform_new_installation_);
    std::set<std::string> keys_to_erase;
    // Filter out the new pre-installed apps for migrating users, so that we
    // don't randomly install them out of the blue. Two-pass to keep iterators
    // nice and happy.
    for (auto entry : prefs) {
      if (!IsOldPreinstalledApp(entry.first))
        keys_to_erase.insert(entry.first);
    }
    for (const auto& key : keys_to_erase)
      prefs.Remove(key);
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
          pref.GetDict().FindString(kWebAppMigrationFlag);
      if (!web_app_flag)
        return false;  // Isn't migrating.
      if (web_app::IsPreinstalledAppInstallFeatureEnabled(*web_app_flag,
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
    for (auto entry : prefs) {
      bool should_re_add = should_re_add_app(entry.first, entry.second);
      if (should_re_add) {
        // Since it will be re-added, mark it as no-longer-migrated.
        web_app::MarkAppAsMigratedToWebApp(profile_, entry.first, false);
      } else {
        keys_to_erase.insert(entry.first);
      }
    }

    for (const auto& key : keys_to_erase) {
      prefs.Remove(key);
    }
  }

  ExternalProviderImpl::SetPrefs(std::move(prefs));
}

}  // namespace preinstalled_apps
