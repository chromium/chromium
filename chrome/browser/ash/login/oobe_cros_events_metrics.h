// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_CROS_EVENTS_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_CROS_EVENTS_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_metrics_helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"

namespace ash {

class OobeCrosEventsMetrics : public OobeMetricsHelper::Observer {
 public:
  explicit OobeCrosEventsMetrics(OobeMetricsHelper* oobe_metrics_helper);
  ~OobeCrosEventsMetrics() override;
  OobeCrosEventsMetrics(const OobeCrosEventsMetrics& other) = delete;
  OobeCrosEventsMetrics& operator=(const OobeCrosEventsMetrics&) = delete;

  void OnPreLoginOobeFirstStarted() override;
  void OnPreLoginOobeCompleted(
      OobeMetricsHelper::CompletedPreLoginOobeFlowType flow_type) override;
  void OnOnboardingStarted() override;
  void OnOnboardingCompleted() override;
  void OnDeviceRegistered() override;
  void OnScreenShownStatusChanged(
      OobeScreenId screen,
      OobeMetricsHelper::ScreenShownStatus status) override;
  void OnScreenExited(OobeScreenId screen,
                      const std::string& exit_reason) override;
  void OnGaiaSignInRequested(GaiaView::GaiaLoginVariant variant) override;
  void OnGaiaSignInCompleted(GaiaView::GaiaLoginVariant variant) override;
  void OnPreLoginOobeResumed(OobeScreenId screen) override;
  void OnOnboardingResumed(OobeScreenId screen) override;
  void OnChoobeResumed() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_CROS_EVENTS_METRICS_H_
