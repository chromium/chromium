// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using apc::ClickAction;
using ClickType = ClickAction::ClickType;
using ClickCount = ClickAction::ClickCount;

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DblClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, RightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_right_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DblRightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextFromFocusedTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_right_click"));
}

}  // namespace

}  // namespace glic::test
