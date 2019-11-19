// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/hashing.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

namespace {

const char kTestGpuProcessName[] = "content_gpu";
const char kTestUtilityProcessName[] = "test_utility_process";

class ChromeStabilityMetricsProviderTest : public testing::Test {
 protected:
  ChromeStabilityMetricsProviderTest() : prefs_(new TestingPrefServiceSimple) {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStabilityMetricsProviderTest);
};

}  // namespace

TEST_F(ChromeStabilityMetricsProviderTest, BrowserChildProcessObserverGpu) {
  base::HistogramTester histogram_tester;
  ChromeStabilityMetricsProvider provider(prefs());

  content::ChildProcessData child_process_data(content::PROCESS_TYPE_GPU);
  child_process_data.metrics_name = kTestGpuProcessName;

  provider.BrowserChildProcessLaunchedAndConnected(child_process_data);
  content::ChildProcessTerminationInfo abnormal_termination_info;
  abnormal_termination_info.status =
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  abnormal_termination_info.exit_code = 1;
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());
  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix("ChildProcess.").empty());
}

TEST_F(ChromeStabilityMetricsProviderTest, BrowserChildProcessObserverUtility) {
  base::HistogramTester histogram_tester;
  ChromeStabilityMetricsProvider provider(prefs());

  content::ChildProcessData child_process_data(content::PROCESS_TYPE_UTILITY);
  child_process_data.metrics_name = kTestUtilityProcessName;

  provider.BrowserChildProcessLaunchedAndConnected(child_process_data);
  const int kExitCode = 1;
  content::ChildProcessTerminationInfo abnormal_termination_info;
  abnormal_termination_info.status =
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  abnormal_termination_info.exit_code = kExitCode;
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());

  // Utility processes also log an entries for the hashed name of the process
  // for launches and crashes.
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Launched.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 2);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessExitCode", kExitCode, 2);
}

TEST_F(ChromeStabilityMetricsProviderTest, NotificationObserver) {
  base::HistogramTester histogram_tester;
  ChromeStabilityMetricsProvider provider(prefs());
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());

  // Owned by profile_manager.
  TestingProfile* profile(
      profile_manager->CreateTestingProfile("StabilityTestProfile"));

  std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory(
      new content::MockRenderProcessHostFactory());
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(profile));

  // Owned by rph_factory.
  content::RenderProcessHost* host(
      rph_factory->CreateRenderProcessHost(profile, site_instance.get()));

  // Crash and abnormal termination should increment renderer crash count.
  content::ChildProcessTerminationInfo crash_details;
  crash_details.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&crash_details));

  content::ChildProcessTerminationInfo term_details;
  term_details.status = base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  term_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&term_details));

  // Kill does not increment renderer crash count.
  content::ChildProcessTerminationInfo kill_details;
  kill_details.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  kill_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&kill_details));

  // Failed launch increments failed launch count.
  content::ChildProcessTerminationInfo failed_launch_details;
  failed_launch_details.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  failed_launch_details.exit_code = 1;
  provider.Observe(content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(host),
                   content::Details<content::ChildProcessTerminationInfo>(
                       &failed_launch_details));

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

#if defined(OS_ANDROID)
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples("Stability.Android.RendererCrash").size());
#else
  EXPECT_EQ(2, system_profile.stability().renderer_crash_count());
#endif
  EXPECT_EQ(1, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  provider.ClearSavedStabilityMetrics();

  // Owned by rph_factory.
  content::RenderProcessHost* extension_host(
      rph_factory->CreateRenderProcessHost(profile, site_instance.get()));

  // Make the rph an extension rph.
  extensions::ProcessMap::Get(profile)
      ->Insert("1", extension_host->GetID(), site_instance->GetId());

  // Crash and abnormal termination should increment extension crash count.
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(extension_host),
      content::Details<content::ChildProcessTerminationInfo>(&crash_details));

  // Failed launch increments failed launch count.
  provider.Observe(content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(extension_host),
                   content::Details<content::ChildProcessTerminationInfo>(
                       &failed_launch_details));

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().extension_renderer_crash_count());
  EXPECT_EQ(
      1, system_profile.stability().extension_renderer_failed_launch_count());
#endif

  profile_manager->DeleteAllTestingProfiles();
}
