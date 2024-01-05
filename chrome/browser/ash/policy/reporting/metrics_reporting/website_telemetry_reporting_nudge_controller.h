// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_WEBSITE_TELEMETRY_REPORTING_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_WEBSITE_TELEMETRY_REPORTING_NUDGE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace reporting {

// Website telemetry reporting nudge id.
inline constexpr char kWebsiteTelemetryReportingNudgeId[] =
    "WebsiteTelemetryReportingNudge";

// Controller for the system nudge notification displayed when website telemetry
// reporting is enabled for the first time since it was last disabled. The
// controller waits until the user session is active before displaying the
// nudge.
class WebsiteTelemetryReportingNudgeController
    : public ::session_manager::SessionManagerObserver {
 public:
  WebsiteTelemetryReportingNudgeController(
      base::WeakPtr<Profile> profile,
      ReportingSettings* reporting_settings);
  WebsiteTelemetryReportingNudgeController(
      const WebsiteTelemetryReportingNudgeController&) = delete;
  WebsiteTelemetryReportingNudgeController& operator=(
      const WebsiteTelemetryReportingNudgeController&) = delete;
  ~WebsiteTelemetryReportingNudgeController() override;

 private:
  // ::session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Initializes the website telemetry reporting nudge controller.
  void Initialize();

  // Displays the nudge notification if website telemetry reporting is enabled
  // for the first time since it was last disabled.
  void MaybeShowNudge();

  // Convenient helper that shows the website telemetry reporting help desk
  // article in a new browser tab.
  void ShowHelpArticle() const;

  // Weak pointer to the user profile. Used to save current setting value to the
  // pref store.
  const base::WeakPtr<Profile> profile_;

  // Reporting settings used to retrieve and observe changes to the website
  // telemetry reporting policy value. Guaranteed to outlive this component
  // because it is owned by the metric reporting manager.
  const raw_ptr<ReportingSettings> reporting_settings_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Callback subscription used while monitoring changes to the website
  // telemetry reporting setting.
  base::CallbackListSubscription change_subscription_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Observer that tracks session state changes so we display the nudge only
  // after the session is active.
  base::ScopedObservation<::session_manager::SessionManager,
                          ::session_manager::SessionManagerObserver>
      session_manager_observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::WeakPtrFactory<WebsiteTelemetryReportingNudgeController>
      weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_WEBSITE_TELEMETRY_REPORTING_NUDGE_CONTROLLER_H_
