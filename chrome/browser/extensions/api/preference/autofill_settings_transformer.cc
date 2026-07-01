// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/autofill_settings_transformer.h"

#include <utility>

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace extensions {

namespace {

constexpr char kUrlPatternExtensionKey[] = "urlPattern";
constexpr char kBlockedTypesExtensionKey[] = "blockedTypes";

}  // namespace

AutofillSettingsTransformer::AutofillSettingsTransformer() = default;

AutofillSettingsTransformer::~AutofillSettingsTransformer() = default;

std::optional<base::Value> AutofillSettingsTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  if (!extension_pref.is_list()) {
    bad_message = true;
    return std::nullopt;
  }

  base::ListValue browser_list;
  for (const auto& item : extension_pref.GetList()) {
    if (!item.is_dict()) {
      bad_message = true;
      return std::nullopt;
    }
    const auto& dict = item.GetDict();
    const std::string* url_pattern = dict.FindString(kUrlPatternExtensionKey);
    const base::ListValue* blocked_types =
        dict.FindList(kBlockedTypesExtensionKey);
    if (!url_pattern || !blocked_types) {
      bad_message = true;
      return std::nullopt;
    }

    // Parse the pattern only for validation. If valid, we store the original
    // string representation rather than the parsed pattern object.
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(*url_pattern);
    if (!pattern.IsValid()) {
      error = "Invalid URL pattern.";
      return std::nullopt;
    }

    browser_list.Append(
        base::DictValue()
            .Set(autofill::prefs::kAutofillBlockedTypesUrlPatternKey,
                 *url_pattern)
            .Set(autofill::prefs::kAutofillBlockedTypesBlockedTypesKey,
                 blocked_types->Clone()));
  }

  return base::Value(std::move(browser_list));
}

std::optional<base::Value> AutofillSettingsTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  if (!browser_pref.is_list()) {
    return std::nullopt;
  }

  base::ListValue extension_list;
  for (const auto& item : browser_pref.GetList()) {
    if (!item.is_dict()) {
      continue;
    }
    const auto& dict = item.GetDict();
    const std::string* url_pattern =
        dict.FindString(autofill::prefs::kAutofillBlockedTypesUrlPatternKey);
    const base::ListValue* blocked_types =
        dict.FindList(autofill::prefs::kAutofillBlockedTypesBlockedTypesKey);
    if (!url_pattern || !blocked_types) {
      continue;
    }

    extension_list.Append(
        base::DictValue()
            .Set(kUrlPatternExtensionKey, *url_pattern)
            .Set(kBlockedTypesExtensionKey, blocked_types->Clone()));
  }

  return base::Value(std::move(extension_list));
}

}  // namespace extensions
