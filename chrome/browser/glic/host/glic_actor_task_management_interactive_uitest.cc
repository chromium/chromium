// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

using apc::ClickAction;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorTaskManagementUiTest : public GlicActorUiTest {
 public:
  GlicActorTaskManagementUiTest() = default;

  // Note that CloseTab does not actually wait for the tab to close, as that is
  // done asynchronously.
  MultiStep CloseTab(ui::ElementIdentifier tab);
};

MultiStep GlicActorTaskManagementUiTest::CloseTab(ui::ElementIdentifier tab) {
  return InAnyContext(WithElement(tab, [this](ui::TrackedElement* el) {
                        content::WebContents* contents =
                            AsInstrumentedWebContents(el)->web_contents();
                        chrome::CloseWebContents(browser(), contents, true);
                      }).SetMustRemainVisible(false));
}

// Ensure that a task can be stopped and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, StopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),
    StopActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
        actor::mojom::ActionResultCode::kTaskWentAway),
    CheckHasTaskForTab(kNewActorTabId, false));
  // clang-format on
}

// Tests that closing a tab that's being acted on stops the associated task.
IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, StopActorTaskOnTabClose) {
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
IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, StopThenStartActTask) {
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
    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kSecondTabId, "() => button_clicked"),
    StopActorTask(),

    // Start, click, stop.
    StartActorTaskInNewTab(task_url, kThirdTabId),
    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kThirdTabId, "() => button_clicked"),
    StopActorTask()
      // clang-format on
  );
}

// Ensure that a task can be paused and that further actions fail.
IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, PauseActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    CheckIsActingOnTab(kNewActorTabId, true),

    PauseActorTask(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE,
                actor::mojom::ActionResultCode::kTaskPaused),

    // Unlike stopping, pausing keeps the task but it is not acting.
    CheckHasTaskForTab(kNewActorTabId, true),
    CheckIsActingOnTab(kNewActorTabId, false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, PauseThenStopActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),
    WaitForActorTaskState(mojom::ActorTaskState::kIdle),

    PauseActorTask(),
    CheckHasTaskForTab(kNewActorTabId, true),
    CheckIsActingOnTab(kNewActorTabId, false),
    WaitForActorTaskState(mojom::ActorTaskState::kPaused),

    StopActorTask(),
    CheckHasTaskForTab(kNewActorTabId, false),
    CheckIsActingOnTab(kNewActorTabId, false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
                       PauseAlreadyPausedActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Ensure pausing twice in a row is a no-op.
    PauseActorTask(),
    PauseActorTask(),
    CheckHasTaskForTab(kNewActorTabId, true),
    CheckIsActingOnTab(kNewActorTabId, false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
                       PauseThenResumeActorTask) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),

    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked"),

    // Reset the flag
    ExecuteJs(kNewActorTabId, "() => { button_clicked = false; }"),

    PauseActorTask(),
    ResumeActorTask(UpdatedContextOptions(), actor::mojom::ActionResultCode::kOk),
    CheckIsActingOnTab(kNewActorTabId, true),

    // Ensure actions work acter pause and resume.
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked")
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
                       PauseThenResumeActorTaskBeforePerformAction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kClickableButtonLabel = "clickable";
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    AddInstrumentedTab(kNewActorTabId, task_url),
    WithElement(kNewActorTabId, [this](ui::TrackedElement* el){
      content::WebContents* tab_contents =
          AsInstrumentedWebContents(el)->web_contents();
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(tab_contents);
      CHECK(tab);
      tab_handle_ = tab->GetHandle();
    }),
    CreateTask(task_id_, ""),
    PauseActorTask(),
    ResumeActorTask(UpdatedContextOptions(), actor::mojom::ActionResultCode::kOk),
    CheckIsActingOnTab(kNewActorTabId, true),
    // Ensure actions work after pause and resume.
    GetPageContextForActorTab(),
    ClickAction(kClickableButtonLabel, ClickAction::LEFT, ClickAction::SINGLE),
    WaitForJsResult(kNewActorTabId, "() => button_clicked")
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
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
    CheckHasTaskForTab(kNewActorTabId, false),

    // Once a task is stopped, it can't be resumed.
    ResumeActorTask(UpdatedContextOptions(), false)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
                       ResumeActorTaskWhenAlreadyResumed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  PauseActorTask(),
                  ResumeActorTask(UpdatedContextOptions(),
                                  actor::mojom::ActionResultCode::kOk),
                  ResumeActorTask(UpdatedContextOptions(), false));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
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
      GetPageContextForActorTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      PauseActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false),
      ResumeActorTask(UpdatedContextOptions(), actor::mojom::ActionResultCode::kOk),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckIsActingOnTab(kNewActorTabId, true),
      CheckIsActingOnTab(kOtherTabId, false),
      CheckHasTaskForTab(kOtherTabId, false),
      StopActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, CreateTaskWithTitle) {
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

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest, CreateTaskNoTitle) {
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

// TODO: win-rel is seeing occasional flakes unrelated to the code being tested.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ForegroundActorTaskTab DISABLED_ForegroundActorTaskTab
#else
#define MAYBE_ForegroundActorTaskTab ForegroundActorTaskTab
#endif
IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementUiTest,
                       MAYBE_ForegroundActorTaskTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL other_url = embedded_test_server()->GetURL("/title1.html");
  base::UserActionTester user_action_tester;

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      WaitForTaskTabForeground(/*expected_foreground=*/true),
      AddInstrumentedTab(kOtherTabId, other_url),
      FocusWebContents(kOtherTabId),
      WaitForTaskTabForeground(/*expected_foreground=*/false),
      ActivateTaskTab(),
      WaitForTaskTabForeground(/*expected_foreground=*/true),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
      "Glic.Instance.TaskTabForegrounded"));
      }));
  // clang-format on
}

class GlicActorTaskManagementDownloadUiTest
    : public GlicActorTaskManagementUiTest {
 public:
  GlicActorTaskManagementDownloadUiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        actor::kGlicDeferDownloadFilePickerToUserTakeover);
  }
  void SetUpOnMainThread() override {
    GlicActorTaskManagementUiTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(::prefs::kPromptForDownload,
                                                 true);

    file_activity_observer_ =
        std::make_unique<DownloadTestFileActivityObserver>(
            browser()->profile());

    file_activity_observer_->EnableFileChooser(true);
  }

  void TearDownOnMainThread() override {
    GlicActorTaskManagementUiTest::TearDownOnMainThread();
    // Needs to be torn down on the main thread. file_activity_observer_ holds a
    // reference to the ChromeDownloadManagerDelegate which should be destroyed
    // on the UI thread.
    file_activity_observer_.reset();
  }

  std::unique_ptr<DownloadTestFileActivityObserver> file_activity_observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementDownloadUiTest,
                       FilePickerDeferredUntilPause) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kDownloadLabel = "download";

  const GURL task_url = embedded_test_server()->GetURL("/actor/download.html");

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserverTerminal> download_observer;
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextForActorTab(),
      ClickAction(kDownloadLabel, ClickAction::LEFT, ClickAction::SINGLE,
                  actor::mojom::ActionResultCode::kFilePickerTriggered),
      WaitForJsResult(kNewActorTabId, "() => download_clicked"),
      CheckResult([&]() { return download_manager->InProgressCount(); }, 1,
                  "A single download should now be in progress"),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          false, "File chooser was not yet shown"),
      Do([&]() {
        download_observer =
            std::make_unique<content::DownloadTestObserverTerminal>(
                download_manager, 1,
                content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
      }),
      PauseActorTask(), Do([&]() { download_observer->WaitForFinished(); }),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          true, "File chooser was shown"),
      CheckResult([&]() { return download_manager->InProgressCount(); }, 0,
                  "The download should have completed"),
      ResumeActorTask(UpdatedContextOptions(),
                      actor::mojom::ActionResultCode::kFilePickerConfirmed),
      CheckIsActingOnTab(kNewActorTabId, true));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementDownloadUiTest,
                       DownloadCancelledWhenTaskStopped) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  constexpr std::string_view kDownloadLabel = "download";

  const GURL task_url = embedded_test_server()->GetURL("/actor/download.html");

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserverTerminal> download_observer;
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextForActorTab(),
      ClickAction(kDownloadLabel, ClickAction::LEFT, ClickAction::SINGLE,
                  actor::mojom::ActionResultCode::kFilePickerTriggered),
      WaitForJsResult(kNewActorTabId, "() => download_clicked"),
      CheckResult([&]() { return download_manager->InProgressCount(); }, 1,
                  "A single download should now be in progress"),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          false, "File picker should be deferred"),
      Do([&]() {
        download_observer =
            std::make_unique<content::DownloadTestObserverTerminal>(
                download_manager, 1,
                content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
      }),
      StopActorTask(), Do([&]() { download_observer->WaitForFinished(); }),
      CheckResult(
          [&]() { return download_manager->InProgressCount(); }, 0,
          "The download should have been cancelled and thus no longer in "
          "progress"),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          false, "The file picker shuold not have shown"));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskManagementDownloadUiTest,
                       AnotherDownloadStartedWhileActorDefersOne) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kDownloadLabel = "download";

  const GURL task_url = embedded_test_server()->GetURL("/actor/download.html");
  const GURL other_url =
      embedded_test_server()->GetURL("example.com", "/actor/download.html");

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserverTerminal> download_observer;
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextForActorTab(),
      ClickAction(kDownloadLabel, ClickAction::LEFT, ClickAction::SINGLE,
                  actor::mojom::ActionResultCode::kFilePickerTriggered),
      WaitForJsResult(kNewActorTabId, "() => download_clicked"),
      CheckResult([&]() { return download_manager->InProgressCount(); }, 1,
                  "A single download should now be in progress"),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          false, "File picker should be deferred"),

      AddInstrumentedTab(kOtherTabId, other_url), Do([&]() {
        download_observer =
            std::make_unique<content::DownloadTestObserverTerminal>(
                download_manager, 1,
                content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
      }),
      WaitForJsResult(kOtherTabId,
                      "() => { document.getElementById('download').click(); "
                      "return true; }"),
      Do([&]() { download_observer->WaitForFinished(); }),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          true, "A file picker should have been shown"),
      CheckResult([&]() { return download_manager->InProgressCount(); }, 1,
                  "Still, only a single download should be in progress"),

      Do([&]() {
        download_observer =
            std::make_unique<content::DownloadTestObserverTerminal>(
                download_manager, 1,
                content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
      }),
      PauseActorTask(), Do([&]() { download_observer->WaitForFinished(); }),
      CheckResult(
          [&]() {
            return file_activity_observer_->TestAndResetDidShowFileChooser();
          },
          true, "File chooser was shown"),
      ResumeActorTask(UpdatedContextOptions(),
                      actor::mojom::ActionResultCode::kFilePickerConfirmed),
      CheckIsActingOnTab(kNewActorTabId, true));
}

}  //  namespace

}  // namespace glic::test
