// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

class Profile;

namespace exo {
class WMHelper;
}

namespace views {
class Widget;
}  // namespace views.

namespace arc {

class ArcAppPerformanceTracing;
class ArcAppPerformanceTracingSession;

// Helper class to share common functionality in browser and unit tests.
class ArcAppPerformanceTracingTestHelper {
 public:
  ArcAppPerformanceTracingTestHelper();
  virtual ~ArcAppPerformanceTracingTestHelper();

  // Creates app window as ARC++ window.
  static views::Widget* CreateArcWindow(const std::string& window_app_id);

  void SetUp(Profile* profile);
  void TearDown();

  // Helper that returns ArcAppPerformanceTracing as service.
  ArcAppPerformanceTracing* GetTracing();

  // Helper that returns active ArcAppPerformanceTracingSession.
  ArcAppPerformanceTracingSession* GetTracingSession();

  // Fires timer to finish statistics tracing or stop waiting for delayed start.
  void FireTimerForTesting();

  // Sends sequence of commits where each commit is delayed for specific delta
  // from |deltas|.
  void PlaySequence(const std::vector<base::TimeDelta>& deltas);

  // Plays default sequence that has FPS = 45, CommitDeviation = 216 and
  // RenderQuality = 48% for target tracing period as 1/3 seconds.
  void PlayDefaultSequence();

 private:
  // Unowned pointer.
  Profile* profile_ = nullptr;

  std::unique_ptr<exo::WMHelper> wm_helper_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingTestHelper);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
