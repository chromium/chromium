// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/ui_base_features.h"
#endif

namespace performance_manager::policies {

namespace {

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

struct PolicyTestParam {
  const MemoryPressureLevel memory_pressure_level =
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE;
  bool tab_backgrounded = false;
  bool enable_policy = true;
  int foreground_cache_size_on_moderate_pressure = 3;
  int background_cache_size_on_moderate_pressure = 2;
  int foreground_cache_size_on_critical_pressure = 1;
  int background_cache_size_on_critical_pressure = 0;
  int expected_cached_pages = 4;
};

const PolicyTestParam kPolicyTestParams[] = {
    // When BFCachePolicy is disabled, the bfcached pages are kept as it is.
    {.enable_policy = false},
    // Tab foregrounded, moderate memory pressure.
    {.expected_cached_pages = 3},
    // Tab backgrounded, moderate memory pressure.
    {.tab_backgrounded = true, .expected_cached_pages = 2},
    // Tab foregrounded, critical memory pressure.
    {.memory_pressure_level =
         MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL,
     .expected_cached_pages = 1},
    // Tab backgrounded, critical memory pressure.
    {.memory_pressure_level =
         MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL,
     .tab_backgrounded = true,
     .expected_cached_pages = 0},
    // Tab foregrounded, moderate memory pressure, cache limit is -1 (no limit).
    {.foreground_cache_size_on_moderate_pressure = -1},
    // Tab backgrounded, moderate memory pressure, cache limit is -1 (no limit).
    {.tab_backgrounded = true,
     .background_cache_size_on_moderate_pressure = -1},
    // Tab foregrounded, critical memory pressure, cache limit is -1 (no limit).
    {.memory_pressure_level =
         MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL,
     .foreground_cache_size_on_critical_pressure = -1},
    // Tab backgrounded, critical memory pressure, cache limit is -1 (no limit).
    {.memory_pressure_level =
         MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL,
     .tab_backgrounded = true,
     .background_cache_size_on_critical_pressure = -1}};

class BFCachePolicyBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<PolicyTestParam> {
 public:
  ~BFCachePolicyBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Occlusion can cause the web_contents to be marked visible between the
    // time the test calls WasHidden and BFCachePolicy::MaybeFlushBFCache is
    // called, which kills the timer set by BFCachePolicy::OnIsVisibleChanged.
#if BUILDFLAG(IS_WIN)
    DisableFeature(::features::kCalculateNativeWinOcclusion);
#endif
    if (GetParam().enable_policy) {
      EnableFeature(
          performance_manager::features::kBFCachePerformanceManagerPolicy,
          {{"foreground_cache_size_on_moderate_pressure",
            base::NumberToString(
                GetParam().foreground_cache_size_on_moderate_pressure)},
           {"background_cache_size_on_moderate_pressure",
            base::NumberToString(
                GetParam().background_cache_size_on_moderate_pressure)},
           {"foreground_cache_size_on_critical_pressure",
            base::NumberToString(
                GetParam().foreground_cache_size_on_critical_pressure)},
           {"background_cache_size_on_critical_pressure",
            base::NumberToString(
                GetParam().background_cache_size_on_critical_pressure)}});
    } else {
      DisableFeature(
          performance_manager::features::kBFCachePerformanceManagerPolicy);
    }

    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            enabled_features_, /*cache_size=*/10, /*foreground_cache_size=*/10),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting(
            disabled_features_));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    // Disable tab discarding to avoid interference.
    mechanism::PageDiscarder::DisableForTesting();
  }

 protected:
  void EnableFeature(const base::Feature& feature,
                     const base::FieldTrialParams& params) {
    enabled_features_.emplace_back(feature, params);
  }

  void DisableFeature(const base::Feature& feature) {
    disabled_features_.emplace_back(feature);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void VerifyEvictionExpectation(
      std::vector<content::RenderFrameHostWrapper>& render_frame_hosts) {
    // Wait for memory pressure signal processed and handled by BFCachePolicy.
    content::RunAllTasksUntilIdle();

    size_t expected_deleted_pages =
        render_frame_hosts.size() - GetParam().expected_cached_pages;

    for (size_t i = 0; i < render_frame_hosts.size(); i++) {
      content::RenderFrameHostWrapper& rfh = render_frame_hosts[i];
      if (i < expected_deleted_pages) {
        ASSERT_TRUE(rfh.IsRenderFrameDeleted());
      } else {
        EXPECT_EQ(
            rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
      }
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
};

}  // namespace

// TODO(https://crbug.com/1335514, https://crbug.com/1494579): Flaky.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_CacheFlushed DISABLED_CacheFlushed
#else
#define MAYBE_CacheFlushed CacheFlushed
#endif
IN_PROC_BROWSER_TEST_P(BFCachePolicyBrowserTest, MAYBE_CacheFlushed) {
  memory_pressure::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor;
  const std::vector<std::string> hostnames{"a.com", "b.com", "c.com", "d.com"};
  std::vector<content::RenderFrameHostWrapper> render_frame_hosts;
  render_frame_hosts.reserve(hostnames.size());
  for (std::string hostname : hostnames) {
    const GURL url(embedded_test_server()->GetURL(hostname, "/title1.html"));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    render_frame_hosts.emplace_back(
        content::RenderFrameHostWrapper(top_frame_host()));
  }

  const GURL url(embedded_test_server()->GetURL("last.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHostWrapper last_rfh =
      content::RenderFrameHostWrapper(top_frame_host());

  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  if (GetParam().tab_backgrounded) {
    web_contents()->WasHidden();
    EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  }

  // Ensure pages are cached.
  for (const auto& render_frame_host : render_frame_hosts) {
    EXPECT_EQ(render_frame_host->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }
  // Ensure the last page is not cached.
  EXPECT_EQ(last_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  fake_memory_pressure_monitor.SetAndNotifyMemoryPressure(
      GetParam().memory_pressure_level);

  VerifyEvictionExpectation(render_frame_hosts);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BFCachePolicyBrowserTest,
                         testing::ValuesIn(kPolicyTestParams));

}  // namespace performance_manager::policies
