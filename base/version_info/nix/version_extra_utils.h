// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VERSION_INFO_NIX_VERSION_EXTRA_UTILS_H_
#define BASE_VERSION_INFO_NIX_VERSION_EXTRA_UTILS_H_

#include <string>

#include "base/base_export.h"
#include "base/version_info/channel.h"

namespace base {
class Environment;
}

namespace version_info::nix {

// The environment variable used to determine the channel.
inline constexpr char kChromeVersionExtra[] = "CHROME_VERSION_EXTRA";

// Returns the channel state for the browser based on the CHROME_VERSION_EXTRA
// environment variable.
BASE_EXPORT version_info::Channel GetChannel(base::Environment& env);

// Returns true if the CHROME_VERSION_EXTRA environment variable is set to
// "extended".
BASE_EXPORT bool IsExtendedStable(base::Environment& env);

// Returns the application name (e.g. "com.google.Chrome.beta" or
// "org.chromium.Chromium").
BASE_EXPORT std::string GetAppName(base::Environment& env);

// Returns the session name prefix (e.g. "chrome_beta" or "chromium").
BASE_EXPORT std::string GetSessionNamePrefix(base::Environment& env);

}  // namespace version_info::nix

#endif  // BASE_VERSION_INFO_NIX_VERSION_EXTRA_UTILS_H_
