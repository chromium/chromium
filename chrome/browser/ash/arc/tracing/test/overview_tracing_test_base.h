// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

class TestingProfile;

namespace exo {
class Surface;
class WMHelper;
}  // namespace exo

namespace arc {

class OverviewTracingTestHandler;

constexpr inline char kBasicSystrace[] =
    "{\"traceEvents\":[],\"systemTraceEvents\":\""
    // clang-format off
    "          <idle>-0     [003] d..0 44442.000001: cpu_idle: state=0 cpu_id=3\n"
    // clang-format on
    "\"}";

class OverviewTracingTestBase : public ash::AshTestBase {
 public:
  OverviewTracingTestBase();

  ~OverviewTracingTestBase() override;

  OverviewTracingTestBase(const OverviewTracingTestBase&) = delete;
  OverviewTracingTestBase& operator=(const OverviewTracingTestBase&) = delete;

  // ash::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Sets the timezone given its ICU name. The original timezone will be
  // restored in the TearDown method.
  static void SetTimeZone(const char* name);

  // Runs commit and present events for `count` frames on `surface`, each
  // separated by `delta`.
  void CommitAndPresentFrames(arc::OverviewTracingTestHandler* handler,
                              exo::Surface* surface,
                              int count,
                              base::TimeDelta delta);

 protected:
  void FastForwardClockAndTaskQueue(arc::OverviewTracingTestHandler* handler,
                                    base::TimeDelta delta);

  // The time relative to which trace tick timestamps are calculated. This is
  // typically when the system was booted.
  base::Time trace_time_base_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  base::FilePath download_path_;
  std::unique_ptr<icu::TimeZone> saved_tz_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_BASE_H_
