// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace arc {

// Observes Arc session manager opt-in status, and records metrics.
class ArcInitialOptInMetricsRecorder : public ArcSessionManagerObserver,
                                       public KeyedService {
 public:
  explicit ArcInitialOptInMetricsRecorder(content::BrowserContext* context);
  ArcInitialOptInMetricsRecorder(const ArcInitialOptInMetricsRecorder&) =
      delete;
  ArcInitialOptInMetricsRecorder& operator=(
      const ArcInitialOptInMetricsRecorder&) = delete;
  ~ArcInitialOptInMetricsRecorder() override;

  // ArcSessionManagerObserver:
  void OnArcOptInUserAction() override;
  void OnArcInitialStart() override;

  // This is called when list of ARC++ apps is updated.
  void OnArcAppListReady();

  // Returns true if we need to report Ash.ArcAppInitialAppsInstallDuration
  // histogram in this session but it has not been reported yet.
  bool NeedReportArcAppListReady() const;

 private:
  std::optional<base::TimeTicks> arc_opt_in_time_;
  bool arc_app_list_ready_reported_ = false;

  // Must be the last member.
  base::WeakPtrFactory<ArcInitialOptInMetricsRecorder> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_METRICS_RECORDER_H_
