// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_provider.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"

namespace extensions {

ComponentExtensionContentSettingsProvider::
    ComponentExtensionContentSettingsProvider(
        ComponentExtensionContentSettingsAllowlist* allowlist)
    : allowlist_(allowlist) {
  // The ComponentExtensionContentSettingsProvider is owned by the
  // HostContentSettingsMap which DependsOn the
  // ComponentExtensionContentSettingsAllowlist (through their factory). This
  // means this will get destroyed before the
  // ComponentExtensionContentSettingsAllowlist and will be unsubscribed from
  // it.
  allowlist_subscription_ =
      allowlist_->SubscribeForContentSettingsChange(base::BindRepeating(
          &ComponentExtensionContentSettingsProvider::OnContentSettingsChange,
          base::Unretained(this)));
}

ComponentExtensionContentSettingsProvider::
    ~ComponentExtensionContentSettingsProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
ComponentExtensionContentSettingsProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  return allowlist_ ? allowlist_->GetRuleIterator(content_type) : nullptr;
}

std::unique_ptr<content_settings::Rule>
ComponentExtensionContentSettingsProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  return allowlist_
             ? allowlist_->GetRule(primary_url, secondary_url, content_type)
             : nullptr;
}

bool ComponentExtensionContentSettingsProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  // ComponentExtensionContentSettingsProvider doesn't support settings Website
  // settings.
  return false;
}

void ComponentExtensionContentSettingsProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  // ComponentExtensionContentSettingsProvider doesn't support changing content
  // settings directly.
}

void ComponentExtensionContentSettingsProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());

  RemoveAllObservers();
  allowlist_subscription_ = {};
  allowlist_ = nullptr;
}

void ComponentExtensionContentSettingsProvider::OnContentSettingsChange(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  NotifyObservers(primary_pattern, secondary_pattern, content_type,
                  /*partition_key=*/nullptr);
}

}  // namespace extensions
