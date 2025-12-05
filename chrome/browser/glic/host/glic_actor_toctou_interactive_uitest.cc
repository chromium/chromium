// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using apc::ClickAction;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorToctouUiTest : public GlicActorUiTest {
 public:
  MultiStep NavigateFrame(ui::ElementIdentifier webcontents_id,
                          const std::string_view frame,
                          const GURL& url);
};

MultiStep GlicActorToctouUiTest::NavigateFrame(
    ui::ElementIdentifier webcontents_id,
    const std::string_view frame,
    const GURL& url) {
  return Steps(ExecuteJs(webcontents_id,
                         base::StrCat({"()=>{document.getElementById('", frame,
                                       "').src='", url.spec(), "';}"})));
}

// TODO(crbug.com/460825984): Re-enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ToctouCheckFailWhenCrossOriginTargetFrameChange \
  DISABLED_ToctouCheckFailWhenCrossOriginTargetFrameChange
#else
#define MAYBE_ToctouCheckFailWhenCrossOriginTargetFrameChange \
  ToctouCheckFailWhenCrossOriginTargetFrameChange
#endif
IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest,
                       MAYBE_ToctouCheckFailWhenCrossOriginTargetFrameChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/two_iframes.html");
  const GURL cross_origin_iframe_url = embedded_test_server()->GetURL(
      "foo.com", "/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    // Initialize the iframes
    ExecuteJs(kNewActorTabId,
              "()=>{topframeLoaded = false; bottomframeLoaded = false;}"),
    NavigateFrame(kNewActorTabId, "topframe", cross_origin_iframe_url),
    NavigateFrame(kNewActorTabId, "bottomframe", cross_origin_iframe_url),
    WaitForJsResult(kNewActorTabId,
                    "()=>{return topframeLoaded && bottomframeLoaded;}"),

    // Click in the top frame. This will extract page context after the click
    // action.
    GetPageContextForActorTab(),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE),

    // Remove the top frame which puts the bottom frame at its former location.
    // Sending a click to the same location should fail the TOCTOU check since
    // the last page context had the removed frame there.
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('topframe').remove();}"),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kFrameLocationChangedSinceObservation)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest,
                       ToctouCheckFailWhenSameSiteTargetFrameChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/two_iframes.html");
  const GURL samesite_iframe_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    // Initialize the iframes
    ExecuteJs(kNewActorTabId,
              "()=>{topframeLoaded = false; bottomframeLoaded = false;}"),
    NavigateFrame(kNewActorTabId, "topframe", samesite_iframe_url),
    NavigateFrame(kNewActorTabId, "bottomframe", samesite_iframe_url),
    WaitForJsResult(kNewActorTabId,
                    "()=>{return topframeLoaded && bottomframeLoaded;}"),

    // Click in the top frame. This will extract page context after the click
    // action.
    GetPageContextForActorTab(),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE),

    // Remove the top frame which puts the bottom frame at its former location.
    // Sending a click to the same location should fail the TOCTOU check since
    // the last page context had the removed frame there.
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('topframe').remove();}"),
    ClickAction(gfx::Point(10, 10), ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kFrameLocationChangedSinceObservation)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest, ToctouCheckFailWhenNodeRemoved) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  constexpr std::string_view kClickableButtonLabel = "clickable";

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('clickable').remove();}"),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
                    actor::mojom::ActionResultCode::kInvalidDomNodeId)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest,
                       ToctouCheckFailForCoordinateTargetWhenNodeMoved) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextForActorTab(),
    ClickAction({15, 15}, ClickAction::LEFT, ClickAction::SINGLE),
    ExecuteJs(kNewActorTabId,
              "()=>{document.getElementById('clickable').style.cssText = "
              "'position: relative; left: 20px;'}"),
    ExecuteJs(kNewActorTabId,
              "()=>{const forcelayout = "
              "document.getElementById('clickable').offsetHeight;}"),
    ClickAction(
        {15, 15}, ClickAction::LEFT, ClickAction::SINGLE,
            actor::mojom::ActionResultCode::kObservedTargetElementChanged)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest,
                       ToctouCheckFailsWhenNodeInteractionPointObscured) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_obscured_element.html");
  constexpr std::string_view kClickableButtonLabel = "target";

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextForActorTab(),
    ClickAction(
        kClickableButtonLabel,
        ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kTargetNodeInteractionPointObscured),
     InAnyContext(WithElement(kNewActorTabId, [](ui::TrackedElement* el) {
        content::WebContents* web_contents =
            AsInstrumentedWebContents(el)->web_contents();
        EXPECT_EQ(false,
          content::EvalJs(web_contents, "target_button_clicked"));
        EXPECT_EQ(false,
          content::EvalJs(web_contents, "obstruction_button_clicked"));
      }))
      // clang-format on
  );
}

// Ensure the time-of-use check can succeed when clicking on a text node rather
// than an element.
IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest, TimeOfUseCheckOnTextNode) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  gfx::Rect checkbox_label_bounds;
  auto click_provider =
      base::BindLambdaForTesting([this, &checkbox_label_bounds]() {
        apc::Actions action =
            actor::MakeClick(tab_handle_, checkbox_label_bounds.CenterPoint(),
                             ClickAction::LEFT, ClickAction::SINGLE);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });
  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, "checkbox-label", checkbox_label_bounds),
      ExecuteAction(std::move(click_provider)),

      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('checkbox').checked")
  );
  // clang-format on
}

// Ensure the time-of-use check can succeed when a click is dispatched to an
// element within a shadow DOM that overlaps its host.
IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest, TimeOfUseCheckOnShadowDom) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  // Load the new page that contains the element with a shadow DOM.
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_shadow_dom.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      GetPageContextForActorTab(),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked === true")
  );
  // clang-format on
}

// Ensure the time-of-use check can succeed when a click is dispatched to a
// multi-line anchor element.
IN_PROC_BROWSER_TEST_F(GlicActorToctouUiTest, TimeOfUseCheckOnMultilineAnchor) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kAnchorLabel = "anchor";

  // Load the new page that contains the multi-line anchor element.
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/multi_line_anchor_element.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ClickAction(kAnchorLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => clicked_fired === true")
  );
  // clang-format on
}

class GlicActorToctouInteractionPointDiscoveryUiTest
    : public GlicActorToctouUiTest {
 public:
  GlicActorToctouInteractionPointDiscoveryUiTest() {
    scoped_features_.InitAndEnableFeature(
        features::kGlicActorIterativeInteractionPointDiscovery);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Ensure time-of-use check doesn't prevent us from clicking on elements
// partially obscured by other elements.
IN_PROC_BROWSER_TEST_F(GlicActorToctouInteractionPointDiscoveryUiTest,
                       TimeOfUseCheckClickBehind) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kBottomElement = "behind";

  // Load the new page that contains the element with a shadow DOM.
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/partially_obscured.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      CheckJsResult(kNewActorTabId,
                    "() => !document.getElementById('behind').checked"),
      ClickAction(kBottomElement,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('behind').checked")
  );
}

}  //  namespace

}  // namespace glic::test
