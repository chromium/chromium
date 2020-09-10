// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_

class PrefRegistrySimple;

namespace base {
class FilePath;
}  // namespace base

namespace version_info {
enum class Channel;
}  // namespace version_info

// These methods are used by ash-chrome.
namespace crosapi {
namespace browser_util {

// Boolean preference. Whether to launch lacros-chrome on login.
extern const char kLaunchOnLoginPref[];

// Registers user profile preferences related to the lacros-chrome binary.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if lacros is allowed for the current user type, chrome channel,
// etc.
bool IsLacrosAllowed();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosAllowed(version_info::Channel channel);

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_
