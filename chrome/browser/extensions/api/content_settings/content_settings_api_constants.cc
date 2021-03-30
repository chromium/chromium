// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"

namespace extensions {
namespace content_settings_api_constants {

// Keys.
const char kContentSettingKey[] = "setting";
const char kContentSettingsTypeKey[] = "type";
const char kDescriptionKey[] = "description";
const char kIdKey[] = "id";
const char kPrimaryPatternKey[] = "primaryPattern";
const char kResourceIdentifierKey[] = "resourceIdentifier";
const char kSecondaryPatternKey[] = "secondaryPattern";

// Errors.
const char kIncognitoContextError[] =
    "Can't modify regular settings from an incognito context.";
const char kIncognitoSessionOnlyError[] =
    "You cannot read incognito content settings when no incognito window "
    "is open.";
const char kInvalidUrlError[] = "The URL \"*\" is invalid.";
}  // namespace content_settings_api_constants
}  // namespace extensions
