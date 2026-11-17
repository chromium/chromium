// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_NAMES_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_NAMES_H_

#include "chrome/common/chrome_version.h"

// Names shared by the macOS local-actuation features: the co-location proof,
// the profile registry and the Mach service that Gemini for macOS uses to talk
// to a running Chrome.

namespace browser_actuator {

// Data Protection Keychain access group that holds the co-location proof and
// the profile registry entries. Chrome shares this group with the local apps
// that read them. The string is the same on every Chrome channel.
//
// Only valid in Google Chrome builds: Chromium's branding leaves
// `MAC_TEAM_IDENTIFIER_STRING` empty, and only the official signing identity
// carries the entitlement for this group.
inline constexpr char kLocalActuationKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING
                               ".local-actuation";

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_NAMES_H_
