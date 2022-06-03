// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Content Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_CONSTANTS_H__
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_CONSTANTS_H__

namespace extensions {
namespace content_settings_api_constants {

// Keys.
extern const char kContentSettingKey[];
extern const char kContentSettingsTypeKey[];
extern const char kDescriptionKey[];
extern const char kIdKey[];
extern const char kPrimaryPatternKey[];
extern const char kResourceIdentifierKey[];
extern const char kSecondaryPatternKey[];

// Errors.
extern const char kIncognitoContextError[];
extern const char kIncognitoSessionOnlyError[];
extern const char kInvalidUrlError[];
}  // namespace content_settings_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_API_CONSTANTS_H__
