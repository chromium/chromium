// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/protected_content_enabled_transformer.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"

namespace extensions {

ProtectedContentEnabledTransformer::ProtectedContentEnabledTransformer() =
    default;
ProtectedContentEnabledTransformer::~ProtectedContentEnabledTransformer() =
    default;

std::optional<base::Value>
ProtectedContentEnabledTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  bool protected_identifier_allowed = extension_pref.GetBool();
  return base::Value(static_cast<int>(protected_identifier_allowed
                                          ? CONTENT_SETTING_ALLOW
                                          : CONTENT_SETTING_BLOCK));
}

std::optional<base::Value>
ProtectedContentEnabledTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  auto protected_identifier_mode =
      static_cast<ContentSetting>(browser_pref.GetInt());
  return base::Value(protected_identifier_mode == CONTENT_SETTING_ALLOW);
}

}  // namespace extensions
