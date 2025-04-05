// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_

class Profile;

namespace glic {

namespace prefs {
enum class FreStatus;
}  // namespace prefs

// Signs in a primary account, accepts the FRE, and eables model execution
// capability for that profile. browser_tests and interactive_ui_tests should
// use GlicTestEnvironment. These methods are for unit_tests.
void ForceSigninAndModelExecutionCapability(Profile* profile);
void SigninWithPrimaryAccount(Profile* profile);
void SetModelExecutionCapability(Profile* profile, bool enabled);
void SetFRECompletion(Profile* profile, prefs::FreStatus fre_status);

void InvalidateAccount(Profile* profile);
void ReauthAccount(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
