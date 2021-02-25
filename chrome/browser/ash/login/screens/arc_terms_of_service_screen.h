// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

class Profile;

namespace chromeos {

class ArcTermsOfServiceScreenView;

class ArcTermsOfServiceScreen : public BaseScreen,
                                public ArcTermsOfServiceScreenViewObserver {
 public:
  enum class Result { ACCEPTED, BACK, NOT_APPLICABLE };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class UserAction {
    kAcceptButtonClicked = 0,
    kNextButtonClicked = 1,
    kRetryButtonClicked = 2,
    kBackButtonClicked = 3,
    kMetricsLearnMoreClicked = 4,
    kBackupRestoreLearnMoreClicked = 5,
    kLocationServiceLearnMoreClicked = 6,
    kPlayAutoInstallLearnMoreClicked = 7,
    kPolicyLinkClicked = 8,
    kMaxValue = kPolicyLinkClicked
  };

  static std::string GetResultString(Result result);

  // Launches the ARC settings page if the user requested to review them after
  // completing OOBE.
  static void MaybeLaunchArcSettings(Profile* profile);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  ArcTermsOfServiceScreen(ArcTermsOfServiceScreenView* view,
                          const ScreenExitCallback& exit_callback);
  ~ArcTermsOfServiceScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // ArcTermsOfServiceScreenViewObserver:
  void OnAccept(bool review_arc_settings) override;
  void OnViewDestroyed(ArcTermsOfServiceScreenView* view) override;

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  ArcTermsOfServiceScreenView* view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
