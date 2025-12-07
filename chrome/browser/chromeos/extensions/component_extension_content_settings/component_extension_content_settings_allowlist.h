// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

// This class is the underlying storage for
// ComponentExtensionContentSettingsProvider, it stores a list of components
// extension ids and their auto-granted permissions. Component extension gets
// auto-granted permissions during loading. This class is created before
// HostContentSettingsMap is registered, since it is created through
// ProfileKeyedServiceFactory it has the same lifetime as the profile it's
// attached to.
class ComponentExtensionContentSettingsAllowlist final
    : public ExtensionRegistryObserver,
      public KeyedService {
 public:
  using ContentSettingsCallbackType =
      void(const ContentSettingsPattern& primary_pattern,
           const ContentSettingsPattern& secondary_pattern,
           ContentSettingsType content_type);
  using ContentSettingsCallback =
      base::RepeatingCallback<ContentSettingsCallbackType>;
  using ContentSettingsCallbackList =
      base::RepeatingCallbackList<ContentSettingsCallbackType>;
  using ExtensionsContentSettingsTypes =
      base::flat_map<ExtensionId, std::vector<ContentSettingsType>>;

  // Convenience function to get the ComponentExtensionContentSettingsAllowlist
  // for a BrowserContext.
  static ComponentExtensionContentSettingsAllowlist* Get(
      content::BrowserContext* context);

  explicit ComponentExtensionContentSettingsAllowlist(
      content::BrowserContext* context);

  ComponentExtensionContentSettingsAllowlist(
      const ComponentExtensionContentSettingsAllowlist&) = delete;
  ComponentExtensionContentSettingsAllowlist& operator=(
      const ComponentExtensionContentSettingsAllowlist&) = delete;

  ~ComponentExtensionContentSettingsAllowlist() override;

  base::CallbackListSubscription SubscribeForContentSettingsChange(
      const ContentSettingsCallback& content_callback);

  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type) const;

  // extensions::ExtensionRegistryObserver implementation
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // KeyedService implementation
  void Shutdown() override;

  static void SetComponentExtensionsContentSettingsTypesForTesting(
      const ExtensionsContentSettingsTypes&
          component_extensions_content_settings_types_for_testing);

 private:
  static const ExtensionsContentSettingsTypes&
  GetComponentExtensionsContentSettingsTypes();

  void SetContentSettingsForComponentExtension(const Extension* extension,
                                               ContentSetting content_setting);
  void SetContentSettingsAndNotifySubscribers(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      ContentSetting content_setting);

  static std::optional<ExtensionsContentSettingsTypes>
      component_extensions_content_settings_types_for_testing_;

  raw_ptr<content::BrowserContext> context_;
  content_settings::OriginValueMap value_map_;
  ContentSettingsCallbackList content_callback_list_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};
};
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_
