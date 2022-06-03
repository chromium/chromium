// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_helpers.h"

#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "content/public/browser/browser_url_handler.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Returns which |extension| (if any) is overriding a particular |type| of
// setting.
const Extension* FindOverridingExtension(
    content::BrowserContext* browser_context,
    SettingsApiOverrideType type) {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context)->enabled_extensions();

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end();
       ++it) {
    const SettingsOverrides* settings = SettingsOverrides::Get(it->get());
    if (settings) {
      if (type == BUBBLE_TYPE_HOME_PAGE && !settings->homepage)
        continue;
      if (type == BUBBLE_TYPE_STARTUP_PAGES && settings->startup_pages.empty())
        continue;
      if (type == BUBBLE_TYPE_SEARCH_ENGINE && !settings->search_engine)
        continue;

      std::string key;
      switch (type) {
        case BUBBLE_TYPE_HOME_PAGE:
          key = prefs::kHomePage;
          break;
        case BUBBLE_TYPE_STARTUP_PAGES:
          key = prefs::kRestoreOnStartup;
          break;
        case BUBBLE_TYPE_SEARCH_ENGINE:
          key = prefs::kDefaultSearchProviderEnabled;
          break;
      }

      // Found an extension overriding the current type, check if primary.
      PreferenceAPI* preference_api = PreferenceAPI::Get(browser_context);
      if (preference_api &&  // Expected to be NULL in unit tests.
          !preference_api->DoesExtensionControlPref((*it)->id(), key, NULL))
        continue;  // Not primary.

      // Found the primary extension.
      return it->get();
    }
  }

  return NULL;
}

}  // namespace

const Extension* GetExtensionOverridingHomepage(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, BUBBLE_TYPE_HOME_PAGE);
}

const Extension* GetExtensionOverridingNewTabPage(
    content::BrowserContext* browser_context) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url, browser_context);
  if (ntp_url.SchemeIs(kExtensionScheme)) {
    return ExtensionRegistry::Get(browser_context)->GetExtensionById(
        ntp_url.host(), ExtensionRegistry::ENABLED);
  }
  return nullptr;
}

const Extension* GetExtensionOverridingStartupPages(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, BUBBLE_TYPE_STARTUP_PAGES);
}

const Extension* GetExtensionOverridingSearchEngine(
    content::BrowserContext* browser_context) {
  return FindOverridingExtension(browser_context, BUBBLE_TYPE_SEARCH_ENGINE);
}

const Extension* GetExtensionOverridingProxy(
    content::BrowserContext* browser_context) {
  ExtensionPrefValueMap* extension_prefs_value_map =
      ExtensionPrefValueMapFactory::GetForBrowserContext(browser_context);
  if (!extension_prefs_value_map)
    return NULL;  // Can be null during testing.
  std::string extension_id =
      extension_prefs_value_map->GetExtensionControllingPref(
          proxy_config::prefs::kProxy);
  if (extension_id.empty())
    return NULL;
  return ExtensionRegistry::Get(browser_context)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED);
}

}  // namespace extensions
