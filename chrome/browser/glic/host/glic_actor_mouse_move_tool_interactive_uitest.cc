// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorMouseMoveToolUiTest : public GlicActorUiTest {
 public:
  MultiStep MouseMoveAction(std::string_view label,
                            actor::TaskId& task_id,
                            tabs::TabHandle& tab_handle,
                            ExpectedErrorResult expected_result = {});

  MultiStep MouseMoveAction(std::string_view label,
                            ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorMouseMoveToolUiTest::MouseMoveAction(
    std::string_view label,
    actor::TaskId& task_id,
    tabs::TabHandle& tab_handle,
    ExpectedErrorResult expected_result) {
  auto move_provider =
      base::BindLambdaForTesting([this, &task_id, &tab_handle, label]() {
        int32_t node_id = SearchAnnotatedPageContent(label);
        content::RenderFrameHost* frame =
            tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
        Actions action = actor::MakeMouseMove(*frame, node_id);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(move_provider), std::move(expected_result));
}

MultiStep GlicActorMouseMoveToolUiTest::MouseMoveAction(
    std::string_view label,
    ExpectedErrorResult expected_result) {
  return MouseMoveAction(label, task_id_, tab_handle_,
                         std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorMouseMoveToolUiTest, NonExistentNode) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }", ""),
      ExecuteAction(
          base::BindLambdaForTesting([this]() {
            content::RenderFrameHost* frame =
                tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
            Actions action =
                actor::MakeMouseMove(*frame, kNonExistentContentNodeId);
            action.set_task_id(task_id_.value());
            return EncodeActionProto(action);
          }),
          actor::mojom::ActionResultCode::kInvalidDomNodeId));
}

IN_PROC_BROWSER_TEST_F(GlicActorMouseMoveToolUiTest, Events) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}", ""),
      MouseMoveAction("first"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseenter[DIV#first],mousemove[DIV#first]"),
      ExecuteJs(kNewActorTabId, "()=>{ event_log = []; }"),
      MouseMoveAction("second"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseleave[DIV#first],mouseenter[DIV#second],mousemove["
                      "DIV#second]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorMouseMoveToolUiTest, TargetOutsideViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}", ""),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY == 0 }"),
      MouseMoveAction("offscreen"),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY > 0 }"),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',')}",
                      "mouseenter[DIV#offscreen],mousemove[DIV#offscreen]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorMouseMoveToolUiTest, MoveToCoordinate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  gfx::Rect first_bounds;
  auto move_provider = base::BindLambdaForTesting([this, &first_bounds]() {
    Actions action =
        actor::MakeMouseMove(tab_handle_, first_bounds.CenterPoint());
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }", ""),
      GetClientRect(kNewActorTabId, "first", first_bounds),
      ExecuteAction(std::move(move_provider)),
      WaitForJsResult(kNewActorTabId, "()=>{ return event_log.join(',') }",
                      "mouseenter[DIV#first],mousemove[DIV#first]"));
}

IN_PROC_BROWSER_TEST_F(GlicActorMouseMoveToolUiTest,
                       MoveToCoordinateOffScreen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  gfx::Rect offscreen_bounds;
  auto move_provider = base::BindLambdaForTesting([this, &offscreen_bounds]() {
    Actions action =
        actor::MakeMouseMove(tab_handle_, offscreen_bounds.CenterPoint());
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, "offscreen", offscreen_bounds),
      ExecuteAction(std::move(move_provider),
                    actor::mojom::ActionResultCode::kCoordinatesOutOfBounds));
}

}  //  namespace

}  // namespace glic::test
