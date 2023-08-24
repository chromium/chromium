// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_UTIL_H_

#include <string>
namespace safe_browsing {

// Strips the filename from the |url|.
std::string SanitizeURLWithoutFilename(std::string url);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_UTIL_H_
