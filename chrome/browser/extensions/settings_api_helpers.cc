// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_helpers.h"

#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "content/public/browser/browser_url_handler.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

enum class OverrideType {
  kStartupPages,
  kHomePage,
  kSearchEngine,
};

// Returns which |extension| (if any) is overriding a particular |type| of
// setting.
const Extension* FindOverridingExtension(
    content::BrowserContext* browser_context,
    OverrideType type) {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context)->enabled_extensions();
  ExtensionPrefsHelper* prefs_helper =
      ExtensionPrefsHelper::Get(browser_context);

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end();
       ++it) {
    const SettingsOverrides* settings = SettingsOverrides::Get(it->get());
    if (settings) {
      if (type == OverrideType::kHomePage && !settings->homepage) {
        continue;
      }
      if (type == OverrideType::kStartupPages &&
          settings->startup_pages.empty()) {
        continue;
      }
      if (type == OverrideType::kSearchEngine && !settings->search_engine) {
        continue;
      }

      std::string key;
      switch (type) {
        case OverrideType::kHomePage:
          key = prefs::kHomePage;
          break;
        case OverrideType::kStartupPages:
          key = prefs::kRestoreOnStartup;
          break;
        case OverrideType::kSearchEngine:
          key = prefs::kDefaultSearchProviderEnabled;
          break;
      }

      // Found an extension overriding the current type, check if primary.
      // ExtensionPrefHelper is not instantiated in unit tests.
      if (prefs_helper &&
          !prefs_helper->DoesExtensionControlPref((*it)->id(), key, nullptr))
        continue;  // Not primary.

      // Found the primary extension.
      return it->get();
    }
  }

  return nullptr;
}

}  // namespace

const Extension* GetExtensionOverridingHomepage(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, OverrideType::kHomePage);
}

const Extension* GetExtensionOverridingNewTabPage(
    content::BrowserContext* browser_context) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url, browser_context);
  if (ntp_url.SchemeIs(kExtensionScheme)) {
    return ExtensionRegistry::Get(browser_context)
        ->enabled_extensions()
        .GetByID(ntp_url.host());
  }
  return nullptr;
}

const Extension* GetExtensionOverridingStartupPages(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, OverrideType::kStartupPages);
}

const Extension* GetExtensionOverridingSearchEngine(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, OverrideType::kSearchEngine);
}

const Extension* GetExtensionOverridingProxy(
    content::BrowserContext* browser_context) {
  ExtensionPrefValueMap* extension_prefs_value_map =
      ExtensionPrefValueMapFactory::GetForBrowserContext(browser_context);
  if (!extension_prefs_value_map)
    return nullptr;  // Can be null during testing.
  std::string extension_id =
      extension_prefs_value_map->GetExtensionControllingPref(
          proxy_config::prefs::kProxy);
  if (extension_id.empty())
    return nullptr;
  return ExtensionRegistry::Get(browser_context)
      ->enabled_extensions()
      .GetByID(extension_id);
}

}  // namespace extensions
