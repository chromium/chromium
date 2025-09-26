// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

using optimization_guide::proto::NavigateAction;

using HistoryDirection = ::actor::HistoryToolRequest::Direction;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorNavigationUiTest : public GlicActorUiTest {
 public:
  MultiStep HistoryAction(actor::HistoryToolRequest::Direction direction,
                          actor::TaskId& task_id,
                          tabs::TabHandle& tab_handle,
                          ExpectedErrorResult expected_result = {});

  MultiStep HistoryAction(actor::HistoryToolRequest::Direction direction,
                          ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorNavigationUiTest::HistoryAction(
    HistoryDirection direction,
    actor::TaskId& task_id,
    tabs::TabHandle& tab_handle,
    ExpectedErrorResult expected_result) {
  auto navigate_provider =
      base::BindLambdaForTesting([&task_id, &tab_handle, direction]() {
        optimization_guide::proto::Actions action =
            direction == HistoryDirection::kBack
                ? actor::MakeHistoryBack(tab_handle)
                : actor::MakeHistoryForward(tab_handle);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(navigate_provider),
                       std::move(expected_result));
}

MultiStep GlicActorNavigationUiTest::HistoryAction(
    HistoryDirection direction,
    ExpectedErrorResult expected_result) {
  return HistoryAction(direction, task_id_, tab_handle_,
                       std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorNavigationUiTest,
                       UsesExistingActorTabOnSubsequentNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL second_navigate_url =
      embedded_test_server()->GetURL("/actor/blank.html?second");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  // Now that the task is started in a new tab, do the
                  // second navigation.
                  NavigateAction(second_navigate_url),
                  WaitForWebContentsReady(kNewActorTabId, second_navigate_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorNavigationUiTest, HistoryTool) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url_1 = embedded_test_server()->GetURL("/actor/blank.html?1");
  const GURL url_2 = embedded_test_server()->GetURL("/actor/blank.html?2");
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(url_1, kNewActorTabId),
    NavigateAction(url_2),
    HistoryAction(HistoryDirection::kBack),
    WaitForWebContentsReady(kNewActorTabId, url_1),
    HistoryAction(HistoryDirection::kForward),
    WaitForWebContentsReady(kNewActorTabId, url_2)
      // clang-format on
  );
}

}  //  namespace

}  // namespace glic::test
