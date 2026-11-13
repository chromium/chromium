// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_ntb_webui_host.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/slim/layer.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

constexpr char kFirstPaintHistogram[] =
    "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint";
constexpr char kWithExistingWindowHistogram[] =
    "Android.TabStrip.NewTabButton.WithExistingWindow."
    "ActivityCreationToFirstPaint";
constexpr char kWithoutExistingWindowHistogram[] =
    "Android.TabStrip.NewTabButton.WithoutExistingWindow."
    "ActivityCreationToFirstPaint";
constexpr char kNavigationStartToCommitHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.NavigationStartToCommit";
constexpr char kCommitToDOMContentLoadedHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.CommitToDOMContentLoaded";
constexpr char kDOMContentLoadedToFirstPaintHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.DOMContentLoadedToFirstPaint";
constexpr char kWithExistingWindowNavigationStartToCommitHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.WithExistingWindow."
    "NavigationStartToCommit";
constexpr char kWithExistingWindowCommitToDOMContentLoadedHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.WithExistingWindow."
    "CommitToDOMContentLoaded";
constexpr char kWithExistingWindowDOMContentLoadedToFirstPaintHistogram[] =
    "Android.TabStrip.NewTabButton.WebUI.WithExistingWindow."
    "DOMContentLoadedToFirstPaint";

class FakePage : public tabstrip_ntb::mojom::Page {
 public:
  void SetButtonVisible(bool visible) override { last_visible_ = visible; }

  mojo::PendingRemote<tabstrip_ntb::mojom::Page> BindAndGetRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  std::optional<bool> last_visible() const { return last_visible_; }

 private:
  mojo::Receiver<tabstrip_ntb::mojom::Page> receiver_{this};
  std::optional<bool> last_visible_;
};

class TabStripNtbWebUiHostTest : public ChromeRenderViewHostTestHarness {
 public:
  TabStripNtbWebUiHostTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // Simulates the commit of the first navigation of `host`, started at
  // `navigation_start`, at the current (mock) time.
  void SimulateCommit(TabStripNtbWebUiHost& host,
                      base::TimeTicks navigation_start) {
    testing::NiceMock<content::MockNavigationHandle> handle(
        host.GetWebContents());
    handle.set_is_in_primary_main_frame(true);
    handle.set_has_committed(true);
    ON_CALL(handle, NavigationStart())
        .WillByDefault(testing::Return(navigation_start));
    host.DidFinishNavigation(&handle);
  }

  void SimulateDOMContentLoaded(TabStripNtbWebUiHost& host) {
    host.DOMContentLoaded(host.GetWebContents()->GetPrimaryMainFrame());
  }
};

TEST_F(TabStripNtbWebUiHostTest, ResetOnTabModelRemovedUnblocksOtrDestruction) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_tab_model = std::make_unique<OwningTestTabModel>(otr_profile);

  TabStripNtbWebUiHost host(otr_profile, base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  host.SetTabModel(otr_tab_model.get());
  ASSERT_NE(nullptr, host.GetWebContents());
  content::WebContentsDestroyedWatcher watcher(host.GetWebContents());

  // Removing an unrelated TabModel (from another profile or the same profile)
  // is a no-op.
  {
    OwningTestTabModel other_tab_model(profile());
    OwningTestTabModel other_otr_tab_model(otr_profile);
  }
  EXPECT_NE(nullptr, host.GetWebContents());

  // Destroying the observed TabModel removes it from TabModelList, which tears
  // down the WebContents and service so the TabModel and OTR Profile can be
  // destroyed before the host itself.
  otr_tab_model.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_EQ(nullptr, host.GetWebContents());
  EXPECT_EQ(nullptr, host.GetLayer());

  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  EXPECT_FALSE(profile()->HasPrimaryOTRProfile());

  // Subsequent calls and host destruction are safe no-ops.
  host.Resize(32, 32);
}

TEST_F(TabStripNtbWebUiHostTest,
       ResetOnProfileWillBeDestroyedUnblocksOtrDestruction) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_tab_model = std::make_unique<OwningTestTabModel>(otr_profile);

  TabStripNtbWebUiHost host(otr_profile, base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  host.SetTabModel(otr_tab_model.get());
  ASSERT_NE(nullptr, host.GetWebContents());
  content::WebContentsDestroyedWatcher watcher(host.GetWebContents());

  // IncognitoTabModelImpl notifies IncognitoProfileDestroyer::didBecomeEmpty()
  // before destroying the delegate TabModel. Verify that
  // DestroyOTRProfileWhenAppropriate tears down the host's WebContents via
  // OnProfileWillBeDestroyed and synchronously destroys the OTR Profile even
  // while the TabModel is still registered.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_EQ(nullptr, host.GetWebContents());
  EXPECT_EQ(nullptr, host.GetLayer());
  EXPECT_FALSE(profile()->HasPrimaryOTRProfile());

  otr_tab_model.reset();
}

TEST_F(TabStripNtbWebUiHostTest,
       ResetOnProfileWillBeDestroyedBeforeSetTabModelUnblocksOtrDestruction) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  TabStripNtbWebUiHost host(otr_profile, base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  ASSERT_NE(nullptr, host.GetWebContents());
  content::WebContentsDestroyedWatcher watcher(host.GetWebContents());

  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_EQ(nullptr, host.GetWebContents());
  EXPECT_EQ(nullptr, host.GetLayer());
  EXPECT_FALSE(profile()->HasPrimaryOTRProfile());

  // Starting the load after the WebContents is gone is a safe no-op.
  host.StartLoad();
}

// Counts the navigations started in a WebContents.
class NavigationStartCounter : public content::WebContentsObserver {
 public:
  explicit NavigationStartCounter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  int count() const { return count_; }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ++count_;
  }

 private:
  int count_ = 0;
};

TEST_F(TabStripNtbWebUiHostTest, DoesNotLoadUntilStartLoad) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
  TabStripNtbWebUiHost host(profile(), base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  content::WebContents* web_contents = host.GetWebContents();
  ASSERT_NE(nullptr, web_contents);
  NavigationStartCounter counter(web_contents);

  // Construction alone must not navigate, so that the WebContents can be
  // attached to its WindowAndroid first.
  EXPECT_EQ(0, counter.count());
  EXPECT_TRUE(web_contents->GetVisibleURL().is_empty());

  host.StartLoad();
  EXPECT_EQ(1, counter.count());
  EXPECT_EQ(GURL(chrome::kChromeUITabStripNtbURL),
            web_contents->GetVisibleURL());
}

TEST_F(TabStripNtbWebUiHostTest, RecordsActivityCreationToFirstPaintOnce) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks activity_creation_time =
      base::TimeTicks() + base::Seconds(10);

  TabStripNtbWebUiHost host(profile(), activity_creation_time,
                            /*java_coordinator=*/nullptr);
  host.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                base::Milliseconds(125));
  histogram_tester.ExpectUniqueTimeSample(kFirstPaintHistogram,
                                          base::Milliseconds(125), 1);
  histogram_tester.ExpectUniqueTimeSample(kWithoutExistingWindowHistogram,
                                          base::Milliseconds(125), 1);
  histogram_tester.ExpectTotalCount(kWithExistingWindowHistogram, 0);

  // Subsequent FCP notifications do not record a second sample.
  host.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                base::Milliseconds(250));
  histogram_tester.ExpectTotalCount(kFirstPaintHistogram, 1);
  histogram_tester.ExpectTotalCount(kWithoutExistingWindowHistogram, 1);
  // Without commit and DOMContentLoaded, no startup phases are recorded.
  histogram_tester.ExpectTotalCount(kNavigationStartToCommitHistogram, 0);

  // A second host for the same profile records the WithExistingWindow
  // partition while the first one is alive, along with its startup phases.
  const base::TimeTicks navigation_start = base::TimeTicks::Now();
  TabStripNtbWebUiHost second(profile(), activity_creation_time,
                              /*java_coordinator=*/nullptr);
  task_environment()->FastForwardBy(base::Milliseconds(20));
  SimulateCommit(second, navigation_start);
  task_environment()->FastForwardBy(base::Milliseconds(30));
  SimulateDOMContentLoaded(second);
  second.OnFirstContentfulPaintInPrimaryMainFrame(base::TimeTicks::Now() +
                                                  base::Milliseconds(10));
  histogram_tester.ExpectTotalCount(kFirstPaintHistogram, 2);
  histogram_tester.ExpectTotalCount(kWithoutExistingWindowHistogram, 1);
  histogram_tester.ExpectTotalCount(kWithExistingWindowHistogram, 1);
  for (const auto& [histogram, with_existing_window_histogram, expected] :
       {std::tuple(kNavigationStartToCommitHistogram,
                   kWithExistingWindowNavigationStartToCommitHistogram,
                   base::Milliseconds(20)),
        std::tuple(kCommitToDOMContentLoadedHistogram,
                   kWithExistingWindowCommitToDOMContentLoadedHistogram,
                   base::Milliseconds(30)),
        std::tuple(kDOMContentLoadedToFirstPaintHistogram,
                   kWithExistingWindowDOMContentLoadedToFirstPaintHistogram,
                   base::Milliseconds(10))}) {
    histogram_tester.ExpectUniqueTimeSample(histogram, expected, 1);
    histogram_tester.ExpectUniqueTimeSample(with_existing_window_histogram,
                                            expected, 1);
  }
}

TEST_F(TabStripNtbWebUiHostTest, ResetHostNoLongerCounts) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks activity_creation_time =
      base::TimeTicks() + base::Seconds(10);
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_tab_model = std::make_unique<OwningTestTabModel>(otr_profile);

  TabStripNtbWebUiHost first(otr_profile, activity_creation_time,
                             /*java_coordinator=*/nullptr);
  first.SetTabModel(otr_tab_model.get());
  // Removing the TabModel resets the host, which unregisters its window.
  otr_tab_model.reset();
  ASSERT_EQ(nullptr, first.GetWebContents());

  TabStripNtbWebUiHost second(otr_profile, activity_creation_time,
                              /*java_coordinator=*/nullptr);
  second.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                  base::Milliseconds(100));
  histogram_tester.ExpectUniqueTimeSample(kWithoutExistingWindowHistogram,
                                          base::Milliseconds(100), 1);
}

TEST_F(TabStripNtbWebUiHostTest, RecreatesRendererWhenKilled) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
  base::HistogramTester histogram_tester;
  const base::TimeTicks activity_creation_time = base::TimeTicks::Now();
  TabStripNtbWebUiHost host(profile(), activity_creation_time,
                            /*java_coordinator=*/nullptr);
  host.StartLoad();
  content::WebContents* web_contents = host.GetWebContents();
  ASSERT_NE(nullptr, web_contents);
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  host.SetButtonVisible(false);

  // The first load commits and reaches DOMContentLoaded, but the renderer dies
  // before the first paint.
  SimulateCommit(host, activity_creation_time);
  SimulateDOMContentLoaded(host);

  // Simulate an unexpected renderer crash.
  auto* rph = static_cast<content::MockRenderProcessHost*>(
      web_contents->GetPrimaryMainFrame()->GetProcess());
  rph->SimulateCrash();
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  // Running the posted reload task recreates the renderer on the same
  // WebContents.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return web_contents->GetPrimaryMainFrame()->IsRenderFrameLive();
  }));
  EXPECT_EQ(web_contents, host.GetWebContents());

  // Button visibility set before the renderer died is preserved on the
  // WebContents and applied when the recreated WebUI binds its page.
  ASSERT_NE(nullptr, web_contents->GetWebUI());
  auto* ntb_ui = web_contents->GetWebUI()
                     ->GetController()
                     ->GetAs<tabstrip_ntb::TabStripNtbUI>();
  ASSERT_NE(nullptr, ntb_ui);
  FakePage page;
  ntb_ui->BindPage(page.BindAndGetRemote());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return page.last_visible().has_value(); }));
  EXPECT_EQ(false, page.last_visible());

  // The renderer exited before the first paint, so the startup phases measure
  // the reload, which produces the first paint.
  const base::TimeTicks reload_start = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  SimulateCommit(host, reload_start);
  task_environment()->FastForwardBy(base::Milliseconds(30));
  SimulateDOMContentLoaded(host);
  host.OnFirstContentfulPaintInPrimaryMainFrame(base::TimeTicks::Now() +
                                                base::Milliseconds(10));
  histogram_tester.ExpectTotalCount(kFirstPaintHistogram, 1);
  histogram_tester.ExpectUniqueTimeSample(kNavigationStartToCommitHistogram,
                                          base::Milliseconds(20), 1);
  histogram_tester.ExpectUniqueTimeSample(kCommitToDOMContentLoadedHistogram,
                                          base::Milliseconds(30), 1);
  histogram_tester.ExpectUniqueTimeSample(
      kDOMContentLoadedToFirstPaintHistogram, base::Milliseconds(10), 1);
}

TEST_F(TabStripNtbWebUiHostTest, RateLimitsRepeatedRendererCrashes) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
  TabStripNtbWebUiHost host(profile(), base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  host.StartLoad();
  content::WebContents* web_contents = host.GetWebContents();
  ASSERT_NE(nullptr, web_contents);
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  auto crash_renderer = [&]() {
    auto* rph = static_cast<content::MockRenderProcessHost*>(
        web_contents->GetPrimaryMainFrame()->GetProcess());
    rph->SimulateCrash();
    EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  };

  // Up to 3 consecutive crashes within the 10s reset window recover
  // immediately via PostTask.
  for (int i = 0; i < 3; ++i) {
    crash_renderer();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return web_contents->GetPrimaryMainFrame()->IsRenderFrameLive();
    }));
  }

  // A 4th crash within 10s is rate-limited and delayed by 1 minute.
  crash_renderer();
  task_environment()->FastForwardBy(base::Seconds(59));
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  // After 10s without a crash, the counter resets and a crash recovers
  // immediately again.
  task_environment()->FastForwardBy(base::Seconds(10));
  crash_renderer();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return web_contents->GetPrimaryMainFrame()->IsRenderFrameLive();
  }));
}

}  // namespace
}  // namespace android
