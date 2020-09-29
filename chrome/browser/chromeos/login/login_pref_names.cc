// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_pref_names.h"

namespace chromeos {

namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile. Here only login/oobe specific prefs
// are presented.

// Last input user method which could be used on the login/lock screens.
const char kLastLoginInputMethod[] = "login.last_input_method";

// Time when new user has finished onboarding.
const char kOobeOnboardingTime[] = "oobe.onboarding_time";

}  // namespace prefs

}  // namespace chromeos
