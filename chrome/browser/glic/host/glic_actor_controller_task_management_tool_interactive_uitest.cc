// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller_interactive_uitest_common.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using apc::ClickAction;
using MultiStep = GlicActorControllerUiTest::MultiStep;

class GlicActorControllerTaskManagementToolUiTest
    : public GlicActorControllerUiTest {
 public:
  // Note that CloseTab does not actually wait for the tab to close, as that is
  // done asynchronously.
  MultiStep CloseTab(ui::ElementIdentifier tab);
};

MultiStep GlicActorControllerTaskManagementToolUiTest::CloseTab(
    ui::ElementIdentifier tab) {
  return InAnyContext(WithElement(tab, [this](ui::TrackedElement* el) {
                        content::WebContents* contents =
                            AsInstrumentedWebContents(el)->web_contents();
                        chrome::CloseWebContents(browser(), contents, true);
                      }).SetMustRemainVisible(false));
}

// Ensure that a task can be stopped and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       StopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),
    StopActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kTaskWentAway),
    CheckIsActingOnTab(kNewActorTabId, false));
  // clang-format on
}

// Tests that closing a tab that's being acted on stops the associated task.
IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       StopActorTaskOnTabClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    CheckIsActingOnTab(kNewActorTabId, true),
    PrepareForStopStateChange(),
    CloseTab(kNewActorTabId),
    WaitForActorTaskStateChangeToStopped());
  // clang-format on
}

// Ensure that a task can be started after a previous task was stopped.
IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       StopThenStartActTask) {
  constexpr std::string_view kClickableButtonLabel = "clickable";
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),

    StartActorTaskInNewTab(task_url, kFirstTabId),
    StopActorTask(),

    // Start, click, stop.
    StartActorTaskInNewTab(task_url, kSecondTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kSecondTabId, "() => button_clicked"),
    StopActorTask(),

    // Start, click, stop.
    StartActorTaskInNewTab(task_url, kThirdTabId),
    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kThirdTabId, "() => button_clicked"),
    StopActorTask()
      // clang-format on
  );
}

// Ensure that a task can be paused and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       PauseActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),

    PauseActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
                actor::mojom::ActionResultCode::kTaskPaused),

    // Unlike stopping, pausing keeps the task.
    CheckIsActingOnTab(kNewActorTabId, true)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       PauseThenStopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    WaitForActorTaskState(mojom::ActorTaskState::kIdle),

    PauseActorTask(),
    CheckIsActingOnTab(kNewActorTabId, true),
    WaitForActorTaskState(mojom::ActorTaskState::kPaused),

    StopActorTask(),
    CheckIsActingOnTab(kNewActorTabId, false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       PauseAlreadyPausedActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Ensure pausing twice in a row is a no-op.
    PauseActorTask(),
    PauseActorTask(),
    CheckIsActingOnTab(kNewActorTabId, true)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       PauseThenResumeActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextFromFocusedTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Reset the flag
    ExecuteJs(kNewActorTabId, "() => { button_clicked = false; }"),

    PauseActorTask(),
    ResumeActorTask(UpdatedContextOptions(), true),
    CheckIsActingOnTab(kNewActorTabId, true),

    // Ensure actions work acter pause and resume.
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked")
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       ResumeActorTaskWithoutATask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    StopActorTask(),
    CheckIsActingOnTab(kNewActorTabId, false),

    // Once a task is stopped, it can't be resumed.
    ResumeActorTask(UpdatedContextOptions(), false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       ResumeActorTaskWhenAlreadyResumed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  PauseActorTask(),
                  ResumeActorTask(UpdatedContextOptions(), true),
                  ResumeActorTask(UpdatedContextOptions(), false));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       ActuationSucceedsOnBackgroundTabAfterPauseAndResume) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      PauseActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false),
      ResumeActorTask(UpdatedContextOptions(), true),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckIsActingOnTab(kNewActorTabId, true),
      CheckIsActingOnTab(kOtherTabId, false),
      StopActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       CreateTaskWithTitle) {
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const std::string task_title = "My test title";

  RunTestSequence(InitializeWithOpenGlicWindow(),    //
                  CreateTask(task_id_, task_title),  //
                  CheckResult(
                      [this]() {
                        const actor::ActorTask* task = GetActorTask();
                        CHECK(task);
                        return task->title();
                      },
                      task_title, "Task has title"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerTaskManagementToolUiTest,
                       CreateTaskNoTitle) {
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),  //
                  CreateTask(task_id_, ""),        //
                  CheckResult(
                      [this]() {
                        const actor::ActorTask* task = GetActorTask();
                        CHECK(task);
                        return task->title();
                      },
                      "", "Task has no title"));
}

}  //  namespace

}  // namespace glic::test
