// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
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

}  // namespace

namespace default_apps {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown);
}

bool Provider::ShouldInstallInProfile() {
  // We decide to install or not install default apps based on the following
  // criteria, from highest priority to lowest priority:
  //
  // - The command line option.  Tests use this option to disable installation
  //   of default apps in some cases.
  // - If the locale is not compatible with the defaults, don't install them.
  // - The kDefaultApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  bool install_apps =
      profile_->GetPrefs()->GetString(prefs::kDefaultApps) == "install";

  InstallState state =
      static_cast<InstallState>(profile_->GetPrefs()->GetInteger(
          prefs::kDefaultAppsInstallState));

  is_migration_ = (state == kProvideLegacyDefaultApps);

  switch (state) {
    case kUnknown: {
      bool is_new_profile = profile_->WasCreatedByVersionOrLater(
          version_info::GetVersionNumber());
      if (!is_new_profile)
        install_apps = false;
      break;
    }

    // The old default apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old default apps to new mechanism where
    // they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case kProvideLegacyDefaultApps:
      profile_->GetPrefs()->SetInteger(
          prefs::kDefaultAppsInstallState,
          kAlreadyInstalledDefaultApps);
      break;

    case kAlreadyInstalledDefaultApps:
    case kNeverInstallDefaultApps:
      install_apps = false;
      break;
    default:
      NOTREACHED();
  }

  if (install_apps && !IsLocaleSupported())
    install_apps = false;

  // Default apps are only installed on profile creation or a new chrome
  // download.
  if (state == kUnknown) {
    if (install_apps) {
      profile_->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                       kAlreadyInstalledDefaultApps);
    } else {
      profile_->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                       kNeverInstallDefaultApps);
    }
  }

  return install_apps;
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   scoped_refptr<extensions::ExternalLoader> loader,
                   extensions::Manifest::Location crx_location,
                   extensions::Manifest::Location download_location,
                   int creation_flags)
    : extensions::ExternalProviderImpl(service,
                                       std::move(loader),
                                       profile,
                                       crx_location,
                                       download_location,
                                       creation_flags),
      profile_(profile),
      is_migration_(false) {
  DCHECK(profile);
  set_auto_acknowledge(true);
}

void Provider::VisitRegisteredExtension() {
  if (!profile_ || !ShouldInstallInProfile()) {
    SetPrefs(std::make_unique<base::DictionaryValue>());
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

void Provider::SetPrefs(std::unique_ptr<base::DictionaryValue> prefs) {
  if (is_migration_) {
    std::set<std::string> new_default_apps;
    for (base::DictionaryValue::Iterator i(*prefs); !i.IsAtEnd(); i.Advance()) {
      if (!IsOldDefaultApp(i.key()))
        new_default_apps.insert(i.key());
    }
    // Filter out the new default apps for migrating users.
    for (auto it = new_default_apps.begin(); it != new_default_apps.end();
         ++it) {
      prefs->Remove(*it, NULL);
    }
  }

  ExternalProviderImpl::SetPrefs(std::move(prefs));
}

}  // namespace default_apps
