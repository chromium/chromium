// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_
#define CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_

class Profile;

namespace glic {

void OpenGlicSettingsPage(Profile* profile);

void OpenGlicOsToggleSetting(Profile* profile);

void OpenGlicKeyboardShortcutSetting(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SETTINGS_UTIL_H_
