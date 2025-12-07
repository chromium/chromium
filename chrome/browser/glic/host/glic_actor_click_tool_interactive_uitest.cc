// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using apc::ClickAction;
using ClickType = ClickAction::ClickType;
using ClickCount = ClickAction::ClickCount;
using apc::BoundingRect;
using apc::ContentNode;

constexpr std::string_view kClickableButtonLabel = "clickable";

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ClickActionWithCoordinatesSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonSelector = "clickable";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  gfx::Rect clickable_button_bounds;

  auto click_provider =
      base::BindLambdaForTesting([&clickable_button_bounds, this]() {
        gfx::Point coordinate = clickable_button_bounds.CenterPoint();
        apc::Actions action =
            actor::MakeClick(tab_handle_, coordinate, apc::ClickAction::LEFT,
                             apc::ClickAction::SINGLE);

        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  GetClientRect(kNewActorTabId, kClickableButtonSelector,
                                clickable_button_bounds),
                  ExecuteAction(std::move(click_provider)),
                  WaitForJsResult(kNewActorTabId, "expect_single_left_click"));
}

// A click on a button in a web component should work, but a click on another
// element in the component should not.
IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ClickActionInWebComponent) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL(
      "/actor/page_with_web_component_button.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                  ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => document.title", "Clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, DblClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::LEFT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_left_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, RightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::SINGLE),
                  WaitForJsResult(kNewActorTabId, "expect_single_right_click"));
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, DblRightClickActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  ClickAction(kClickableButtonLabel, ClickAction::RIGHT,
                              ClickAction::DOUBLE),
                  WaitForJsResult(kNewActorTabId, "expect_double_right_click"));
}

}  // namespace

}  // namespace glic::test
