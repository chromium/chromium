// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorScrollToToolUiTest : public GlicActorUiTest {
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
          Actions action = actor::MakeScrollTo(*frame, node_id);
          action.set_task_id(task_id.value());
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

          Actions action = actor::MakeScrollTo(*frame, node_id);
          action.set_task_id(task_id_.value());
          return EncodeActionProto(action);
        });

    return ExecuteAction(std::move(scroll_provider),
                         std::move(expected_result));
  }
};

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest, FailsOnInvalidNodeID) {
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

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest, ScrollsToValidNodeID) {
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
                      "}"),

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

IN_PROC_BROWSER_TEST_F(GlicActorScrollToToolUiTest, DisplayNoneDoesNotScroll) {
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
