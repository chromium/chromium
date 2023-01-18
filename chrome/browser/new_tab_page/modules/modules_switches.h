// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_SWITCHES_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_SWITCHES_H_

namespace switches {

// Enables NTP modules on the signed out user experience. Used for perf tests,
// which do not support sign-in.
extern const char kSignedOutNtpModulesSwitch[];

}  // namespace switches

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_SWITCHES_H_
