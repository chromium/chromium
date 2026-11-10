// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeTracingDelegateAndroidTest,
     DisallowsRecordingWhenOffTheRecordTabModelAdded) {
  content::BrowserTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  ChromeTracingDelegate delegate(*local_state);
  TestingProfile regular_profile;

  task_environment.FastForwardBy(base::Seconds(1));
  const base::TimeTicks session_start = base::TimeTicks::Now();
  task_environment.FastForwardBy(base::Seconds(1));

  OwningTestTabModel regular_model(&regular_profile);
  EXPECT_TRUE(delegate.IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false), session_start));

  {
    OwningTestTabModel otr_model(
        regular_profile.GetPrimaryOTRProfile(/*create_if_needed=*/true));
    EXPECT_FALSE(delegate.IsRecordingAllowed(
        content::TracingDelegate::IsLocalScenario(false), session_start));
  }

  EXPECT_FALSE(delegate.IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false), session_start));

  task_environment.FastForwardBy(base::Seconds(1));
  const base::TimeTicks next_session_start = base::TimeTicks::Now();
  EXPECT_TRUE(delegate.IsRecordingAllowed(
      content::TracingDelegate::IsLocalScenario(false), next_session_start));
}
