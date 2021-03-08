// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
#define CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_

#include <memory>

#include "base/values.h"

namespace chromeos {

class UserContext;

// Structure that defines data that need to be passed between screens during
// WizardController flows.
class WizardContext {
 public:
  WizardContext();
  ~WizardContext();

  WizardContext(const WizardContext&) = delete;
  WizardContext& operator=(const WizardContext&) = delete;

  // Configuration for automating OOBE screen actions, e.g. during device
  // version rollback.
  // Set by WizardController.
  // Used by multiple screens.
  base::Value configuration{base::Value::Type::DICTIONARY};

  // Indicates that enterprise enrollment was triggered early in the OOBE
  // process, so Update screen should be skipped and Enrollment start right
  // after EULA screen.
  // Set by Welcome, Network and EULA screens.
  // Used by Update screen and WizardController.
  bool enrollment_triggered_early = false;

  // Indicates that user selects to sign in or create a new account for a child.
  bool sign_in_as_child = false;

  // Indicates whether user creates a new gaia account when set up the device
  // for a child.
  bool is_child_gaia_account_new = false;

  // Whether the screens should be skipped so that the normal gaia login is
  // shown. Set by WizardController SkipToLoginForTesting and checked on
  // EnrollmentScreen::MaybeSkip (determines if enrollment screen should be
  // skipped when enrollment isn't mandatory) and UserCreationScreen::MaybeSkip
  // (determines if user creation screen should be skipped).
  bool skip_to_login_for_tests = false;

  // Whether wizard controller should skip to the update screen. Setting this
  // flag will ignore hid detection results.
  bool skip_to_update_for_tests = false;

  // Whether user creation screen is enabled (could be disabled due to disabled
  // feature or on managed device). It determines the behavior of back button
  // for GaiaScreen and OfflineLoginScreen. Value is set to true in
  // UserCreationScreen::MaybeSkip when screen is shown and will be set to false
  // when screen is skipped or when cancel action is called.
  bool is_user_creation_enabled = false;

  // Authorization data that is required by PinSetup screen to add PIN as
  // another possible auth factor. Can be empty (if PIN is not supported).
  // In future will be replaced by AuthSession.
  std::unique_ptr<UserContext> extra_factors_auth_session;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
