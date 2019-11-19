// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H__
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H__

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace extensions {
namespace content_settings_helpers {

// Parses an extension match pattern and returns a corresponding
// content settings pattern object.
// If |pattern_str| is invalid or can't be converted to a content settings
// pattern, |error| is set to the parsing error and an invalid pattern
// is returned.
ContentSettingsPattern ParseExtensionPattern(const std::string& pattern_str,
                                             std::string* error);

// Converts a content settings type string to the corresponding
// ContentSettingsType. Returns ContentSettingsType::DEFAULT if the string
// didn't specify a valid content settings type.
ContentSettingsType StringToContentSettingsType(
    const std::string& content_type);
// Returns a string representation of a ContentSettingsType.
std::string ContentSettingsTypeToString(ContentSettingsType type);

}  // namespace content_settings_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H__
