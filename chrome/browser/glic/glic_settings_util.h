// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_
#define CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_

#include <string>
#include <string_view>

#include "url/gurl.h"

class Profile;

namespace glic {

void OpenGlicSettingsPage(Profile* profile);

void OpenGlicOsToggleSetting(Profile* profile);

void OpenGlicKeyboardShortcutSetting(Profile* profile);

void OpenPasswordManagerSettingsPage(Profile* profile);

// Returns a GURL derived from `url_string` that has platform-specific
// parameters appended to the "p" query parameter if it exists.
GURL GetHelpCenterUrl(std::string_view url_string);

// Returns the platform-specific suffix for Glic help center URLs (e.g.,
// "_win").
std::string_view GetPlatformHelpSuffix();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_
