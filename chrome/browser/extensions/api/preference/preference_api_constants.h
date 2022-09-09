// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_CONSTANTS_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_CONSTANTS_H__

namespace extensions {
namespace preference_api_constants {

// Keys for incoming arguments.
extern const char kIncognitoKey[];
extern const char kScopeKey[];

// Keys for returned parameters.
extern const char kIncognitoSpecific[];
extern const char kLevelOfControl[];
extern const char kValue[];

// Errors.
extern const char kIncognitoErrorMessage[];
extern const char kIncognitoSessionOnlyErrorMessage[];
extern const char kPermissionErrorMessage[];
extern const char kPrimaryProfileOnlyErrorMessage[];

}  // namespace preference_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_CONSTANTS_H__
