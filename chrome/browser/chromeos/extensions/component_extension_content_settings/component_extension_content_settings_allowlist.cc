// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"

#include "base/callback_list.h"
#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist_factory.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace extensions {

namespace {
bool IsExtension(const url::Origin& origin) {
  return origin.scheme() == kExtensionScheme;
}

bool IsComponentExtensionOrigin(const url::Origin& origin) {
  return IsComponentExtensionAllowlisted(origin.host());
}
}  // namespace

// static
ComponentExtensionContentSettingsAllowlist*
ComponentExtensionContentSettingsAllowlist::Get(
    content::BrowserContext* context) {
  return ComponentExtensionContentSettingsAllowlistFactory::
      GetForBrowserContext(context);
}

ComponentExtensionContentSettingsAllowlist::
    ComponentExtensionContentSettingsAllowlist() = default;
ComponentExtensionContentSettingsAllowlist::
    ~ComponentExtensionContentSettingsAllowlist() = default;

base::CallbackListSubscription
ComponentExtensionContentSettingsAllowlist::SubscribeForContentSettingsChange(
    const ContentSettingsCallback& content_settings_callback) {
  return content_callback_list_.Add(content_settings_callback);
}

void ComponentExtensionContentSettingsAllowlist::RegisterAutoGrantedPermission(
    const url::Origin& origin,
    ContentSettingsType content_type,
    ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(IsExtension(origin));
  CHECK(IsComponentExtensionOrigin(origin));

  SetContentSettingsAndNotifySubscribers(
      ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
      ContentSettingsPattern::Wildcard(), content_type, content_setting);
}

void ComponentExtensionContentSettingsAllowlist::RegisterAutoGrantedPermissions(
    const url::Origin& origin,
    std::initializer_list<ContentSettingsType> content_types,
    ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& content_type : content_types) {
    RegisterAutoGrantedPermission(origin, content_type, content_setting);
  }
}

std::unique_ptr<content_settings::RuleIterator>
ComponentExtensionContentSettingsAllowlist::GetRuleIterator(
    ContentSettingsType content_type) const {
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule>
ComponentExtensionContentSettingsAllowlist::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  base::AutoLock lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type);
}

void ComponentExtensionContentSettingsAllowlist::
    SetContentSettingsAndNotifySubscribers(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern,
        ContentSettingsType content_type,
        ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  {
    base::AutoLock auto_lock(value_map_.GetLock());
    if (!value_map_.SetValue(primary_pattern, secondary_pattern, content_type,
                             base::Value(content_setting), /*metadata=*/{})) {
      return;
    }
  }

  // Notify subscribers
  content_callback_list_.Notify(primary_pattern, secondary_pattern,
                                content_type);
}

}  // namespace extensions
