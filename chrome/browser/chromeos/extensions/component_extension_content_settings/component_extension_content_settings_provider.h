// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_PROVIDER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace extensions {

class ComponentExtensionContentSettingsAllowlist;

// A provider that supplies HostContentSettingsMap with a list of auto-granted
// permissions from the underlying
// ComponentExtensionContentSettingsAllowList.
class ComponentExtensionContentSettingsProvider final
    : public content_settings::ObservableProvider {
 public:
  explicit ComponentExtensionContentSettingsProvider(
      ComponentExtensionContentSettingsAllowlist* allowlist);

  ComponentExtensionContentSettingsProvider(
      const ComponentExtensionContentSettingsProvider&) = delete;
  ComponentExtensionContentSettingsProvider& operator=(
      const ComponentExtensionContentSettingsProvider&) = delete;

  ~ComponentExtensionContentSettingsProvider() override;

  // content_settings::ObservableProvider implementation
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record) const override;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record) const override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;

 private:
  void OnContentSettingsChange(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type);

  raw_ptr<ComponentExtensionContentSettingsAllowlist> allowlist_;
  base::CallbackListSubscription allowlist_subscription_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_COMPONENT_EXTENSION_CONTENT_SETTINGS_COMPONENT_EXTENSION_CONTENT_SETTINGS_PROVIDER_H_
