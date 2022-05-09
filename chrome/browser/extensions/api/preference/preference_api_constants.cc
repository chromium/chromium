// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api_constants.h"

namespace extensions {
namespace preference_api_constants {

const char kIncognitoKey[] = "incognito";

const char kScopeKey[] = "scope";

const char kIncognitoSpecific[] = "incognitoSpecific";
const char kLevelOfControl[] = "levelOfControl";
const char kValue[] = "value";

const char kIncognitoErrorMessage[] =
    "You do not have permission to access incognito preferences.";

const char kIncognitoSessionOnlyErrorMessage[] =
    "You cannot set a preference with scope 'incognito_session_only' when no "
    "incognito window is open.";

const char kPermissionErrorMessage[] =
    "You do not have permission to access the preference '*'. "
    "Be sure to declare in your manifest what permissions you need.";

const char kPrimaryProfileOnlyErrorMessage[] =
    "You may only access this preference in the primary profile.";

}  // namespace preference_api_constants
}  // namespace extensions
