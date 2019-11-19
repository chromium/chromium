// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_custom_extension_provider.h"

#include <memory>

#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

CustomExtensionProvider::CustomExtensionProvider(
    const scoped_refptr<extensions::ContentSettingsStore>& extensions_settings,
    bool incognito)
    : incognito_(incognito), extensions_settings_(extensions_settings) {
  extensions_settings_->AddObserver(this);
}

CustomExtensionProvider::~CustomExtensionProvider() {
}

std::unique_ptr<RuleIterator> CustomExtensionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return extensions_settings_->GetRuleIterator(content_type,
                                               resource_identifier,
                                               incognito);
}

bool CustomExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::Value>&& value) {
  return false;
}

void CustomExtensionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
  extensions_settings_->RemoveObserver(this);
}

void CustomExtensionProvider::OnContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  if (incognito_ != incognito)
    return;
  // TODO(markusheintz): Be more concise.
  NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                  ContentSettingsType::DEFAULT, std::string());
}

}  // namespace content_settings
