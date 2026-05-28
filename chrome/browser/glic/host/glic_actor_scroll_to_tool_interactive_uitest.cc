// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point.h"

namespace glic::test {

namespace {

using Actions = ::optimization_guide::proto::Actions;
using ExpectedErrorResult = GlicActorUiTest::ExpectedErrorResult;
using MultiStep = GlicActorUiTest::MultiStep;

// Base helper class containing shared action generators for ScrollTo tests.
class GlicActorScrollToToolUiTestBase : public GlicActorUiTest {
 public:
  MultiStep ScrollToAction(std::string_view label,
                           actor::TaskId& task_id,
                           tabs::TabHandle& tab_handle,
                           ExpectedErrorResult expected_result = {}) {
    auto scroll_provider =
        base::BindLambdaForTesting([this, &task_id, &tab_handle, label]() {
          const int32_t node_id = SearchAnnotatedPageContent(label);
          content::RenderFrameHost* frame =
              tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
          Actions action = actor::MakeScrollTo(*frame, node_id, task_id);
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(scroll_provider),
                         std::move(expected_result));
  }

  MultiStep ScrollToAction(std::string_view label,
                           ExpectedErrorResult expected_result = {}) {
    return ScrollToAction(label, task_id_, tab_handle_,
                          std::move(expected_result));
  }

  // Executes a scroll-to action. If `query_selector` is empty, it attempts
  // to scroll to a non-existent node (kNonExistentContentNodeId), which is
  // useful for testing error cases. Otherwise, it scrolls to the element
  // identified by the `query_selector`.
  MultiStep ExecuteScrollToActionWithNodeId(
      std::string_view query_selector = "",
      ExpectedErrorResult expected_result = {}) {
    auto scroll_provider =
        base::BindLambdaForTesting([this, query_selector]() -> std::string {
          content::RenderFrameHost* frame =
              tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
          int node_id;

          if (query_selector.empty()) {
            node_id = kNonExistentContentNodeId;
          } else {
            node_id = content::GetDOMNodeId(*frame, query_selector).value();
          }

          Actions action = actor::MakeScrollTo(*frame, node_id, task_id_);
          return EncodeActionProto(action);
        });
    return ExecuteAction(std::move(scroll_provider),
                         std::move(expected_result));
  }
};

// Test fixture with kGlicActorToctouValidation enabled (default).
class GlicActorScrollToToolUiTest : public GlicActorScrollToToolUiTestBase {};

// Test fixture with kGlicActorToctouValidation disabled.
// This is necessary for tests that target arbitrary DOM nodes directly
// (using NodeId instead of APC node labels) which are not present in the APC
// observation, as they would otherwise fail TOCTOU validation closed.
//
// NOTE: It's not clear if it's intentional that targeted nodes are not in APC.
// We could update these tests to guarantee that and simplify this test suite.
class GlicActorScrollToToolValidationDisabledUiTest
    : public GlicActorScrollToToolUiTestBase {
 public:
  GlicActorScrollToToolValidationDisabledUiTest() {
    feature_list_.InitAndDisableFeature(features::kGlicActorToctouValidation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolValidationDisabledUiTest,
                       FailsOnInvalidNodeID) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ExecuteScrollToActionWithNodeId(
                      /*query_selector=*/"",
                      actor::mojom::ActionResultCode::kInvalidDomNodeId),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollX", 0),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

// Test scrolling to an element that exists in the APC observation with TOCTOU
// enabled.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest, ScrollsToValidApcNodeID) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");

  const std::string kInViewportLabel = "in-viewport";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),

      // Scroll to an element already in the viewport.
      ScrollToAction(kInViewportLabel),
      WaitForJsResult(kNewActorTabId,
                      "() => {"
                      "  const rect = document.getElementById('in-viewport')"
                      "    .getBoundingClientRect();"
                      "  return window.innerWidth >= rect.right && "
                      "    window.innerHeight >= rect.bottom;"
                      "}"));
}

// Test scrolling to an arbitrary DOM node that is NOT in the APC observation.
// This requires TOCTOU validation to be disabled.
// TODO(crbug.com/460810821): Flaky on Mac and ChromeOS.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_ScrollsToValidNonApcNodeID DISABLED_ScrollsToValidNonApcNodeID
#else
#define MAYBE_ScrollsToValidNonApcNodeID ScrollsToValidNonApcNodeID
#endif
IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolValidationDisabledUiTest,
                       MAYBE_ScrollsToValidNonApcNodeID) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),

      // Scroll to an element outside of the viewport.
      ExecuteScrollToActionWithNodeId("#out-of-viewport"),
      WaitForJsResult(
          kNewActorTabId,
          "() => {"
          "  const rect = document.getElementById('out-of-viewport')"
          "    .getBoundingClientRect();"
          "  return window.innerWidth >= rect.right && "
          "    window.innerHeight >= rect.bottom;"
          "}"));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest,
                       PositionFixedDoesNotScroll) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");
  const std::string kFixedElementLabel = "fixed";

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ScrollToAction(kFixedElementLabel),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollX", 0),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolValidationDisabledUiTest,
                       DisplayNoneDoesNotScroll) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ExecuteScrollToActionWithNodeId(
          "#display-none", actor::mojom::ActionResultCode::kElementOffscreen),
      WaitForJsResult(kNewActorTabId, "() => window.scrollX", 0),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest,
                       OffScreenPositionFixedDoesNotScroll) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/scroll_to.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ExecuteScrollToActionWithNodeId(
                      "#offscreen-fixed",
                      actor::mojom::ActionResultCode::kElementOffscreen),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollX", 0),
                  WaitForJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

}  // namespace

}  // namespace glic::test
