// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_ENUMS_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_ENUMS_H_

namespace extensions {
namespace settings_private {

// Success or error statuses from calling PrefsUtil::SetPref.
enum class SetPrefResult {
  SUCCESS,
  PREF_NOT_MODIFIABLE,
  PREF_NOT_FOUND,
  PREF_TYPE_MISMATCH,
  PREF_TYPE_UNSUPPORTED
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_ENUMS_H_
