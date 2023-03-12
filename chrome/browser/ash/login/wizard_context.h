// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
#define CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"

namespace ash {

class UserContext;

// Structure that defines data that need to be passed between screens during
// WizardController flows.
class WizardContext {
 public:
  WizardContext();
  ~WizardContext();

  WizardContext(const WizardContext&) = delete;
  WizardContext& operator=(const WizardContext&) = delete;

  // Should be tweaked by the tests only in case we need this early in the init
  // process. Otherwise tweak context from `GetWizardContextForTesting`.
  static bool g_is_branded_build;

  enum class EnrollmentPreference {
    kKiosk,
    kEnterprise,
  };

  struct RecoverySetup {
    // Whether the recovery auth factor is supported. Used for metrics.
    bool is_supported = false;

    // Controls if user should be asked about recovery factor setup
    // on the consolidated consent screen.
    bool ask_about_recovery_consent = false;

    // User's choice about using recovery factor. Filled by
    // consolidated consent screen, used by auth_factors_setup screen.
    bool recovery_factor_opted_in = false;
  };

  // Configuration for automating OOBE screen actions, e.g. during device
  // version rollback.
  // Set by WizardController.
  // Used by multiple screens.
  base::Value::Dict configuration;

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

  // Whether the post login screens should be skipped. Used in MaybeSkip by
  // screens in tests. Is set by WizardController::SkipPostLoginScreensForTests.
  bool skip_post_login_screens_for_tests = false;

  // Whether user creation screen is enabled (could be disabled due to disabled
  // feature or on managed device). It determines the behavior of back button
  // for GaiaScreen and OfflineLoginScreen. Value is set to true in
  // UserCreationScreen::MaybeSkip when screen is shown and will be set to false
  // when screen is skipped or when cancel action is called.
  bool is_user_creation_enabled = false;

  // When --tpm-is-dynamic switch is set taking TPM ownership is happening right
  // before enrollment. If TakeOwnership returns STATUS_DEVICE_ERROR this
  // flag helps to set the right error message.
  bool tpm_owned_error = false;

  // When --tpm-is-dynamic switch is set taking TPM ownership is happening right
  // before enrollment. If TakeOwnership returns STATUS_DBUS_ERROR this
  // flag helps to set the right error message.
  bool tpm_dbus_error = false;

  // True if this is a branded build (i.e. Google Chrome).
  bool is_branded_build = g_is_branded_build;

  // Force that OOBE Login display isn't destroyed right after login due to all
  // screens being skipped.
  bool defer_oobe_flow_finished_for_tests = false;

  // Indicates which type of licenses should be used for primary button on
  // enrollment screen.
  EnrollmentPreference enrollment_preference_ =
      WizardContext::EnrollmentPreference::kEnterprise;

  // The data for recovery setup flow.
  RecoverySetup recovery_setup;

  // Authorization data that is required by PinSetup screen to add PIN as
  // another possible auth factor. Can be empty (if PIN is not supported).
  // In future will be replaced by AuthSession.
  std::unique_ptr<UserContext> extra_factors_auth_session;

  // If the onboarding flow wasn't completed by the user we will try to show
  // TermsOfServiceScreen to them first and then continue the flow with this
  // screen. If the user has already completed onboarding, but
  // TermsOfServiceScreen should be shown on login this will be set to
  // ash::OOBE_SCREEN_UNKNOWN.
  OobeScreenId screen_after_managed_tos;

  // This ID maps onto the instance_id used in
  // ash::multidevice::RemoteDevice. If a user connects their phone during Quick
  // Start, Quick Start saves this ID. After Quick Start, the multidevice screen
  // will show UI enhancements if this quick_start_phone_instance_id is present.
  std::string quick_start_phone_instance_id;

  // If this is a first login after update from CloudReady to a new version.
  // During such an update show users license agreement and data collection
  // consent.
  bool is_cloud_ready_update_flow = false;

  // Determining ownership can take some time. Instead of finding out if the
  // current user is an owner of the device we reuse this value. It is set
  // during ConsolidatedConsentScreen.
  absl::optional<bool> is_owner_flow;

  // True when gesture navigation screen was shown during the OOBE.
  bool is_gesture_navigation_screen_was_shown = false;

  // True when user is inside the "Add Person" flow.
  bool is_add_person_flow = false;

  // True if user clicked "Select more fatures" button on the last CHOOBE
  // selected screen.
  bool return_to_choobe_screen = false;

  // Information that is used during Cryptohome recovery or password changed
  // flow.
  std::unique_ptr<UserContext> user_context;

  // Indicates whether there is error when fetching Gaia reauth request token.
  // This flag helps us determine the reason when the reauth proof token is
  // missing and if we should ask the user to login again.
  bool gaia_reauth_token_fetch_error = false;
};

// Returns |true| if this is an OOBE flow after enterprise enrollment.
bool IsRollbackFlow(const WizardContext& context);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
