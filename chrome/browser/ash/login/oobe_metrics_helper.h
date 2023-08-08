// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_

#include <map>

#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_screen.h"

namespace ash {

// Handles metrics for OOBE.
class OobeMetricsHelper {
 public:
  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class ScreenShownStatus { kSkipped = 0, kShown = 1, kMaxValue = kShown };

  // The type of flow completed when pre-login OOBE is completed.
  enum class CompletedPreLoginOobeFlowType {
    kAutoEnrollment = 0,
    kDemo = 1,
    kRegular = 2
  };

  OobeMetricsHelper();
  ~OobeMetricsHelper();
  OobeMetricsHelper(const OobeMetricsHelper& other) = delete;
  OobeMetricsHelper& operator=(const OobeMetricsHelper&) = delete;

  // Called when the status of a screen during the flow is determined,
  // shown/skipped.
  void OnScreenShownStatusDetermined(OobeScreenId screen,
                                     ScreenShownStatus status);

  // Called when the screen is exited, this should be preceded by a call to
  // `OnScreenShownStatusDetermined()`.
  void OnScreenExited(OobeScreenId screen, const std::string& exit_reason);

  // Called upon marking pre-login OOBE as completed.
  void OnPreLoginOobeCompleted(CompletedPreLoginOobeFlowType screen);

  // Called when `ShowEnrollmentScreen()` is called.
  void OnEnrollmentScreenShown();

  void RecordChromeVersion();

 private:
  // Maps screen names to last time of their shows.
  std::map<OobeScreenId, base::TimeTicks> screen_show_times_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_METRICS_HELPER_H_
