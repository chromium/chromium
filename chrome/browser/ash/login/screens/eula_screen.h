// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_EULA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_EULA_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "url/gurl.h"

namespace ash {

class EulaView;

// Representation independent class that controls OOBE screen showing EULA
// to users.
class EulaScreen : public BaseScreen {
 public:
  enum class Result {
    // The user accepted EULA, and enabled usage stats reporting.
    ACCEPTED_WITH_USAGE_STATS_REPORTING,
    // The user accepted EULA, and disabled usage stats reporting.
    ACCEPTED_WITHOUT_USAGE_STATS_REPORTING,
    // Eula already accepted, skip screen
    ALREADY_ACCEPTED,
    // Eula already accepted, skip screen (demo mode)
    ALREADY_ACCEPTED_DEMO_MODE,
    // The user did not accept EULA - they clicked back button instead.
    BACK,
    // The user did not accept EULA - they clicked back button instead (demo
    // mode).
    BACK_DEMO_MODE,
    // Eula screen is skipped.
    NOT_APPLICABLE,
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class UserAction {
    kAcceptButtonClicked = 0,
    kBackButtonClicked = 1,
    kShowAdditionalTos = 2,
    kShowSecuritySettings = 3,
    kShowStatsUsageLearnMore = 4,
    kUnselectStatsUsage = 5,
    kSelectStatsUsage = 6,
    kMaxValue = kSelectStatsUsage
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  EulaScreen(base::WeakPtr<EulaView> view,
             const ScreenExitCallback& exit_callback);

  EulaScreen(const EulaScreen&) = delete;
  EulaScreen& operator=(const EulaScreen&) = delete;

  ~EulaScreen() override;

  // Returns URL of the OEM EULA page that should be displayed using current
  // locale and manifest. Returns empty URL otherwise.
  GURL GetOemEulaUrl() const;

  void SetUsageStatsEnabled(bool enabled);

  // Returns true if usage statistics reporting is enabled.
  bool IsUsageStatsEnabled() const;

 protected:
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // EulaView:
  void ShowStatsUsageLearnMore();
  void ShowAdditionalTosDialog();
  void ShowSecuritySettingsDialog();

  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  base::WeakPtr<EulaView> view_;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_EULA_SCREEN_H_
