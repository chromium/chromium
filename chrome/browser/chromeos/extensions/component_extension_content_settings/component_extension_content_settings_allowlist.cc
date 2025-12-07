// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist_factory.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {
static const ComponentExtensionContentSettingsAllowlist::
    ExtensionsContentSettingsTypes kComponentExtensionsContentSettingsTypes = {
        {extension_misc::kQuickOfficeComponentExtensionId,
         {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
          ContentSettingsType::FILE_SYSTEM_WRITE_GUARD}}};
}  // namespace

std::optional<
    ComponentExtensionContentSettingsAllowlist::ExtensionsContentSettingsTypes>
    ComponentExtensionContentSettingsAllowlist::
        component_extensions_content_settings_types_for_testing_;

ComponentExtensionContentSettingsAllowlist*
ComponentExtensionContentSettingsAllowlist::Get(
    content::BrowserContext* context) {
  return ComponentExtensionContentSettingsAllowlistFactory::
      GetForBrowserContext(context);
}

ComponentExtensionContentSettingsAllowlist::
    ComponentExtensionContentSettingsAllowlist(content::BrowserContext* context)
    : context_(context) {
  observation_.Observe(extensions::ExtensionRegistry::Get(context_));
}

ComponentExtensionContentSettingsAllowlist::
    ~ComponentExtensionContentSettingsAllowlist() = default;

base::CallbackListSubscription
ComponentExtensionContentSettingsAllowlist::SubscribeForContentSettingsChange(
    const ContentSettingsCallback& content_settings_callback) {
  return content_callback_list_.Add(content_settings_callback);
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

void ComponentExtensionContentSettingsAllowlist::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  SetContentSettingsForComponentExtension(
      extension, ContentSetting::CONTENT_SETTING_ALLOW);
}

void ComponentExtensionContentSettingsAllowlist::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  SetContentSettingsForComponentExtension(
      extension, ContentSetting::CONTENT_SETTING_DEFAULT);
}

void ComponentExtensionContentSettingsAllowlist::Shutdown() {
  content_callback_list_.Clear();
}

void ComponentExtensionContentSettingsAllowlist::
    SetComponentExtensionsContentSettingsTypesForTesting(
        const ExtensionsContentSettingsTypes&
            component_extensions_content_settings_types_for_testing) {
  component_extensions_content_settings_types_for_testing_ =
      component_extensions_content_settings_types_for_testing;
}

const ComponentExtensionContentSettingsAllowlist::
    ExtensionsContentSettingsTypes&
    ComponentExtensionContentSettingsAllowlist::
        GetComponentExtensionsContentSettingsTypes() {
  return component_extensions_content_settings_types_for_testing_.has_value()
             ? component_extensions_content_settings_types_for_testing_.value()
             : kComponentExtensionsContentSettingsTypes;
}

void ComponentExtensionContentSettingsAllowlist::
    SetContentSettingsForComponentExtension(const Extension* extension,
                                            ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& component_extensions_content_settings_types =
      GetComponentExtensionsContentSettingsTypes().find(extension->id());
  if (component_extensions_content_settings_types ==
          GetComponentExtensionsContentSettingsTypes().end() ||
      !IsComponentExtensionAllowlisted(extension->id())) {
    return;
  }

  for (const auto& content_type :
       component_extensions_content_settings_types->second) {
    SetContentSettingsAndNotifySubscribers(
        ContentSettingsPattern::FromURLNoWildcard(extension->url()),
        ContentSettingsPattern::Wildcard(), content_type, content_setting);
  }
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
    if (content_setting == CONTENT_SETTING_DEFAULT) {
      // CONTENT_SETTING_DEFAULT cannot be set through SetValue
      if (!value_map_.DeleteValue(primary_pattern, secondary_pattern,
                                  content_type)) {
        return;
      }
    } else if (!value_map_.SetValue(primary_pattern, secondary_pattern,
                                    content_type, base::Value(content_setting),
                                    /*metadata=*/{})) {
      return;
    }
  }

  // Notify subscribers
  content_callback_list_.Notify(primary_pattern, secondary_pattern,
                                content_type);
}

}  // namespace extensions
