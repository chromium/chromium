// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_

#include "chrome/browser/ui/browser.h"

class AmbientAuthenticationTestHelper {
 public:
  AmbientAuthenticationTestHelper() = default;
  static bool IsAmbientAuthAllowedForProfile(Profile* profile);
  static bool IsIncognitoAllowedInPolicy(int policy_value);
  static bool IsGuestAllowedInPolicy(int policy_value);
  static Profile* GetGuestProfile();
  // OpenGuestBrowser method code borrowed from
  // chrome/browser/profiles/profile_window_browsertest.cc
  static Browser* OpenGuestBrowser();
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
