// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_TEST_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_TEST_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_

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

namespace arc {

class ArcAppPerformanceTracing;
class ArcAppPerformanceTracingSession;

enum class PresentType {
  kDiscarded,
  kSuccessful,
};

// Helper class to share common functionality in browser and unit tests.
class ArcAppPerformanceTracingTestHelper {
 public:
  ArcAppPerformanceTracingTestHelper();

  ArcAppPerformanceTracingTestHelper(
      const ArcAppPerformanceTracingTestHelper&) = delete;
  ArcAppPerformanceTracingTestHelper& operator=(
      const ArcAppPerformanceTracingTestHelper&) = delete;

  virtual ~ArcAppPerformanceTracingTestHelper();

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
  void PlaySequence(exo::Surface* surface,
                    const std::vector<base::TimeDelta>& deltas);

  // Plays default sequence that has PerceivedFPS = FPS = 48,
  // CommitDeviation = 216 and RenderQuality = 48% for target tracing period as
  // 1/3 seconds.
  void PlayDefaultSequence(exo::Surface* surface);

  // Causes the surface to be committed and its present callback to be invoked.
  void Commit(exo::Surface* surface, PresentType present);

  // Disables App Syncing for profile.
  void DisableAppSync();

  void AdvanceTickCount(base::TimeDelta delta);

  base::TimeTicks ticks_now() { return ticks_now_; }

 private:
  // Unowned pointer.
  raw_ptr<Profile> profile_ = nullptr;

  // Timestamps used in generated commits.
  base::TimeTicks ticks_now_ =
      base::TimeTicks() + base::Microseconds(42'000'042);

  std::unique_ptr<exo::WMHelper> wm_helper_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_TEST_ARC_APP_PERFORMANCE_TRACING_TEST_HELPER_H_
