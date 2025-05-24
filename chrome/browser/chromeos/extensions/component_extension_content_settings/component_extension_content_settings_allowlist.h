// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_

#include <initializer_list>
#include <memory>

#include "base/callback_list.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace extensions {

// This class is the underlying storage for
// ComponentExtensionContentSettingsProvider, it stores a list of origins and
// permissions to be auto-granted to component extensions. This class is created
// before HostContentSettingsMap is registered, since it is created through
// ProfileKeyedServiceFactory it has the same lifetime as the profile it's
// attached to.
class ComponentExtensionContentSettingsAllowlist final : public KeyedService {
 public:
  using ContentSettingsCallbackType =
      void(const ContentSettingsPattern& primary_pattern,
           const ContentSettingsPattern& secondary_pattern,
           ContentSettingsType content_type);
  using ContentSettingsCallback =
      base::RepeatingCallback<ContentSettingsCallbackType>;
  using ContentSettingsCallbackList =
      base::RepeatingCallbackList<ContentSettingsCallbackType>;

  // Convenience function to get the ComponentExtensionContentSettingsAllowlist
  // for a BrowserContext.
  static ComponentExtensionContentSettingsAllowlist* Get(
      content::BrowserContext* context);

  ComponentExtensionContentSettingsAllowlist();
  ~ComponentExtensionContentSettingsAllowlist() override;

  ComponentExtensionContentSettingsAllowlist(
      const ComponentExtensionContentSettingsAllowlist&) = delete;
  ComponentExtensionContentSettingsAllowlist& operator=(
      const ComponentExtensionContentSettingsAllowlist&) = delete;

  base::CallbackListSubscription SubscribeForContentSettingsChange(
      const ContentSettingsCallback& content_callback);

  // Register auto-granted `content_type` permission for component extensions
  // `origin`.
  //
  // ComponentExtensionContentSettingsAllowlist comes with no permission by
  // default. These allowlist help to get permissions to component extensions.
  void RegisterAutoGrantedPermission(
      const url::Origin& origin,
      ContentSettingsType content_type,
      ContentSetting content_setting = ContentSetting::CONTENT_SETTING_ALLOW);

  // Register auto-granted `content_types` permissions for `origin`. See
  // RegisterAutoGrantedPermission comment.
  void RegisterAutoGrantedPermissions(
      const url::Origin& origin,
      std::initializer_list<ContentSettingsType> content_types,
      ContentSetting content_setting = ContentSetting::CONTENT_SETTING_ALLOW);

  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type) const;

 private:
  void SetContentSettingsAndNotifySubscribers(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      ContentSetting content_setting);

  content_settings::OriginValueMap value_map_;
  ContentSettingsCallbackList content_callback_list_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_ALLOWLIST_H_
