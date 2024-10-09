// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/auth/cryptohome_pin_engine.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {

class PinSetupScreenView;
class WizardContext;

class PinSetupScreen : public BaseScreen {
 public:
  using TView = PinSetupScreenView;
  enum class Result {
    kDoneAsSecondaryFactor = 0,
    kUserSkip,
    kNotApplicable,
    kNotApplicableAsPrimaryFactor,
    kTimedOut,
    kUserChosePassword,
    kDoneAsMainFactor,
  };

  // Detailed reason describing why the screen is being skipped.
  enum class SkipReason {
    kSkippedForTests = 0,
    kNotAllowedByPolicy,
    kMissingExtraFactorsToken,
    kExpiredToken,
    kManagedGuestSessionOrEphemeralLogin,
    kUsupportedHardware,
    kNotSupportedAsPrimaryFactor,
    kPinAlreadySet,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class UserAction {
    kDoneButtonClicked = 0,
    kSkipButtonClickedOnStart = 1,
    kSkipButtonClickedInFlow = 2,
    kMaxValue = kSkipButtonClickedInFlow
  };

  // Whether the current platform has support for PIN login, or just unlock.
  // Initially undetermined until PinBackend informs us of the actual state.
  enum class HardwareSupport { kLoginCompatible, kUnlockOnly };

  static std::string GetResultString(Result result);

  static std::unique_ptr<base::AutoReset<bool>>
  SetForceNoSkipBecauseOfPolicyForTests(bool value);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  PinSetupScreen(base::WeakPtr<PinSetupScreenView> view,
                 const ScreenExitCallback& exit_callback);

  PinSetupScreen(const PinSetupScreen&) = delete;
  PinSetupScreen& operator=(const PinSetupScreen&) = delete;

  ~PinSetupScreen() override;

  // Test methods below
  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  // Checks if the screen should be skipped by returning a detailed reason.
  std::optional<PinSetupScreen::SkipReason> GetSkipReason(
      WizardContext& context);

  // Finalizes the hardware support status.
  void DetermineHardwareSupport();

  void OnHasLoginSupport(bool login_available);
  void OnTokenTimedOut();

  // Hardware support and screen mode. The main logic bits driving how the
  // screen is surfaced to the user. See enum definition for details.
  std::optional<HardwareSupport> hardware_support_;

  base::WeakPtr<PinSetupScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::OneShotTimer token_lifetime_timeout_;

  AuthPerformer auth_performer_;

  legacy::CryptohomePinEngine cryptohome_pin_engine_;

  // For keeping the AuthSession while offering PIN as a main factor.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  base::WeakPtrFactory<PinSetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
