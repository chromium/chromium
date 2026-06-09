// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::NavigateIframeToURL;
using content::RenderFrameHost;
using content::WebContents;

namespace actor {
namespace {

class PageToolToctouPopupBypassTest : public ActorToolsTest {
 public:
  PageToolToctouPopupBypassTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActorToctouValidation},
        /*disabled_features=*/{kGlicCrossOriginNavigationGating,
                               features::kGlicActorUi});
  }

  void SetUp() override {
    EnablePixelOutput();
    ActorToolsTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SimulateModelObservation(tabs::TabInterface* tab) {
    base::RunLoop loop;
    auto options = optimization_guide::ActionableAIPageContentOptions(
        /*on_critical_path=*/true);
    optimization_guide::GetAIPageContent(
        tab->GetContents(), std::move(options),
        base::BindOnce(
            [](tabs::TabInterface* tab, base::OnceClosure quit,
               optimization_guide::AIPageContentResultOrError result) {
              ASSERT_TRUE(result.has_value());
              if (auto* tab_data = ActorTabData::From(tab)) {
                tab_data->DidObserveContent(result->proto,
                                            actor::ApcSource::kActor);
              }
              std::move(quit).Run();
            },
            tab, loop.QuitClosure()));
    loop.Run();
  }

  gfx::Point SetUpAttackAndSwap(tabs::TabInterface* tab) {
    WebContents* wc = tab->GetContents();
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS, Ash window decorations (caption bar and window borders)
    // overlap the WebContents view area. This causes the main browser window
    // surface to be aggregated in Viz with the 'Ask' flag (due to
    // OverlappedRegion/IrregularClip properties). Since FindWidgetAtPoint is
    // synchronous, it cannot perform asynchronous queries to Aura when it hits
    // an 'Ask' region, and fails to route coordinates to cross-process
    // subframes. Forcing the window to fullscreen removes these decorations and
    // the 'Ask' flag, allowing synchronous hit-testing to successfully traverse
    // child FrameSinks.
    Browser* browser = nullptr;
    if (tab->GetBrowserWindowInterface()) {
      browser = tab->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();
    }
    if (browser && !browser->GetWindow()->IsFullscreen()) {
      ui_test_utils::ToggleFullscreenModeAndWait(browser);
    }
#endif

    const GURL attacker = embedded_https_test_server().GetURL(
        "a.com", "/actor/toctou_frame_swap.html");
    EXPECT_TRUE(content::NavigateToURL(wc, attacker));

    const GURL victim = embedded_https_test_server().GetURL(
        "b.com", "/actor/page_with_clickable_element.html");
    EXPECT_TRUE(NavigateIframeToURL(wc, "victim", victim));

    RenderFrameHost* victim_frame = ChildFrameAt(wc->GetPrimaryMainFrame(), 0);
    EXPECT_TRUE(victim_frame);
    EXPECT_TRUE(victim_frame->IsCrossProcessSubframe());

    SimulateModelObservation(tab);
    gfx::Point click_pt(150, 100);

    content::RenderWidgetHost* victim_rwh = victim_frame->GetRenderWidgetHost();

    EXPECT_TRUE(ExecJs(wc, "swap_victim_under_decoy()"));

    // Poll FindWidgetAtPoint until hit testing routes to the victim frame.
    // This is required because after executing the JS swap, the layout and
    // compositor updates occur asynchronously in the renderer process. The
    // renderer submits a new CompositorFrame to the Viz process, which must
    // then aggregate the new hit-test region list and dispatch it back to the
    // browser process's HitTestQuery database. We must poll synchronously here
    // to wait until the browser-side Viz hit-test router has actively received
    // and registered these new iframe boundaries, resolving the
    // compositor-to-browser synchronization race.
    bool hit_test_ok = false;
    for (int i = 0; i < 50; ++i) {
      if (wc->FindWidgetAtPoint(gfx::PointF(click_pt)) == victim_rwh) {
        hit_test_ok = true;
        break;
      }
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(20));
      run_loop.Run();
    }
    EXPECT_TRUE(hit_test_ok) << "Swapped iframe never received hit-test!";
    return click_pt;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageToolToctouPopupBypassTest,
                       NormalWindow_FrameSwapIsBlocked) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);
  ASSERT_TRUE(tab->IsInNormalWindow());
  ASSERT_NE(ActorTabData::From(tab), nullptr);

  gfx::Point click_pt = SetUpAttackAndSwap(tab);

  std::unique_ptr<ToolRequest> click = MakeClickRequest(*tab, click_pt);
  ActResultFuture result;
  actor_task().Act(ToRequestList(click), result.GetCallback());

  ExpectErrorResult(
      result, mojom::ActionResultCode::kFrameLocationChangedSinceObservation);

  RenderFrameHost* victim_frame =
      ChildFrameAt(tab->GetContents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(false, EvalJs(victim_frame, "button_clicked"));
}

IN_PROC_BROWSER_TEST_F(PageToolToctouPopupBypassTest,
                       PopupWindow_FrameSwapIsBlockedAfterFix) {
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_FALSE(popup->is_type_normal());
  tabs::TabInterface* popup_tab = popup->GetActiveTabInterface();
  ASSERT_TRUE(popup_tab);
  ASSERT_FALSE(popup_tab->IsInNormalWindow());

  // Verify fix: ActorTabData now exists for popup tabs.
  ASSERT_NE(ActorTabData::From(popup_tab), nullptr);

  gfx::Point click_pt = SetUpAttackAndSwap(popup_tab);

  std::unique_ptr<ToolRequest> click = MakeClickRequest(*popup_tab, click_pt);
  ActResultFuture result;
  actor_task().Act(ToRequestList(click), result.GetCallback());

  // Verify fix: Mitigation now fires for popup tabs.
  ExpectErrorResult(
      result, mojom::ActionResultCode::kFrameLocationChangedSinceObservation);

  RenderFrameHost* victim_frame =
      ChildFrameAt(popup_tab->GetContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(victim_frame);
  EXPECT_TRUE(victim_frame->IsCrossProcessSubframe());
  EXPECT_EQ(false, EvalJs(victim_frame, "button_clicked"))
      << "Click was delivered to the cross-origin victim frame, bug still "
         "exists!";
}

}  // namespace
}  // namespace actor
