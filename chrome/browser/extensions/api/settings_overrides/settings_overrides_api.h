// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

#include <memory>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

BASE_DECLARE_FEATURE(kPrepopulatedSearchEngineOverrideRollout);

class SettingsOverridesAPI : public BrowserContextKeyedAPI,
                             public ExtensionRegistryObserver {
 public:
  explicit SettingsOverridesAPI(content::BrowserContext* context);

  SettingsOverridesAPI(const SettingsOverridesAPI&) = delete;
  SettingsOverridesAPI& operator=(const SettingsOverridesAPI&) = delete;

  ~SettingsOverridesAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SettingsOverridesAPI>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SettingsOverridesAPI>;

  // Wrappers around PreferenceAPI.
  void SetPref(const ExtensionId& extension_id,
               const std::string& pref_key,
               base::Value value) const;
  void UnsetPref(const ExtensionId& extension_id,
                 const std::string& pref_key) const;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  void RegisterSearchProvider(const Extension* extension) const;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SettingsOverridesAPI"; }

  raw_ptr<Profile> profile_;
  raw_ptr<TemplateURLService> url_service_;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

template <>
void BrowserContextKeyedAPIFactory<
    SettingsOverridesAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
