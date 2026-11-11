// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_ntb_webui_host.h"

#include <memory>
#include <optional>

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
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {
namespace {

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
  histogram_tester.ExpectUniqueTimeSample(
      "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint",
      base::Milliseconds(125), 1);

  // Subsequent FCP notifications do not record a second sample.
  host.OnFirstContentfulPaintInPrimaryMainFrame(activity_creation_time +
                                                base::Milliseconds(250));
  histogram_tester.ExpectTotalCount(
      "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint", 1);
}

TEST_F(TabStripNtbWebUiHostTest, RecreatesRendererWhenKilled) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kAndroidNewTabButtonTabstripWebUI);
  TabStripNtbWebUiHost host(profile(), base::TimeTicks(),
                            /*java_coordinator=*/nullptr);
  host.StartLoad();
  content::WebContents* web_contents = host.GetWebContents();
  ASSERT_NE(nullptr, web_contents);
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  host.SetButtonVisible(false);

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
