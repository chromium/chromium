// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_

#include <string>

class Profile;
namespace base {
class DictionaryValue;
}

namespace extension_settings_helper {

// Calls Set() with |settings| for |profile| and the extension with ID |id|.
void SetExtensionSettings(Profile* profile,
                          const std::string& id,
                          const base::DictionaryValue& settings);

// Calls Set() with |settings| for all profiles the extension with ID |id|.
void SetExtensionSettingsForAllProfiles(const std::string& id,
                                        const base::DictionaryValue& settings);

// Returns whether the extension settings are the same across all profiles.
bool AllExtensionSettingsSameAsVerifier();

}  // namespace extension_settings_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_
