// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chromeos/tpm/tpm_password_fetcher.h"
#include "url/gurl.h"

namespace chromeos {

class EulaView;

// Representation independent class that controls OOBE screen showing EULA
// to users.
//
// TODO(b/168852740): Clean up the password-related logic with the removal of
// |TpmGetPassword| D-Bus method.
class EulaScreen : public BaseScreen, public TpmPasswordFetcherDelegate {
 public:
  enum class Result {
    // The user accepted EULA, and enabled usage stats reporting.
    ACCEPTED_WITH_USAGE_STATS_REPORTING,
    // The user accepted EULA, and disabled usage stats reporting.
    ACCEPTED_WITHOUT_USAGE_STATS_REPORTING,
    // The usage did not accept EULA - they clicked back button instead.
    BACK
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
  EulaScreen(EulaView* view, const ScreenExitCallback& exit_callback);
  ~EulaScreen() override;

  // Returns URL of the OEM EULA page that should be displayed using current
  // locale and manifest. Returns empty URL otherwise.
  GURL GetOemEulaUrl() const;

  // Initiate TPM password fetch. Will call view's OnPasswordFetched() when
  // done.
  void InitiatePasswordFetch();

  void SetUsageStatsEnabled(bool enabled);

  // Returns true if usage statistics reporting is enabled.
  bool IsUsageStatsEnabled() const;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call SetModel(NULL).
  void OnViewDestroyed(EulaView* view);

 protected:
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;
  bool HandleAccelerator(ash::LoginAcceleratorAction action) override;

  // TpmPasswordFetcherDelegate implementation:
  void OnPasswordFetched(const std::string& tpm_password) override;
  void ShowStatsUsageLearnMore();
  void ShowAdditionalTosDialog();

  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  EulaView* view_;

  ScreenExitCallback exit_callback_;

  TpmPasswordFetcher password_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
