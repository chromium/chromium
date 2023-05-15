// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class Profile;

namespace exo {
class Surface;
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

  ArcAppPerformanceTracingTestHelper(
      const ArcAppPerformanceTracingTestHelper&) = delete;
  ArcAppPerformanceTracingTestHelper& operator=(
      const ArcAppPerformanceTracingTestHelper&) = delete;

  virtual ~ArcAppPerformanceTracingTestHelper();

  // Creates app window as ARC++ window.
  // Caller retains ownership of |shell_root_surface|.
  // If |shell_root_surface| is not given or is nullptr, one will be created,
  // which should be cleaned up by the surface tree destruction.
  static views::Widget* CreateArcWindow(
      const std::string& window_app_id,
      exo::Surface* shell_root_surface = nullptr);

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

  // Disables App Syncing for profile.
  void DisableAppSync();

 private:
  // Unowned pointer.
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;

  std::unique_ptr<exo::WMHelper> wm_helper_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
