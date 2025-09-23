// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_controller_interactive_uitest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_platform.h"

namespace glic::test {

namespace {

using MultiStep = GlicActorControllerUiTest::MultiStep;

class GlicActorControllerWindowManagementUiTest
    : public GlicActorControllerUiTest {
 public:
  MultiStep CreateWindowAction(actor::TaskId& task_id,
                               ExpectedErrorResult expected_result = {});

  MultiStep ActivateWindowAction(actor::TaskId& task_id,
                                 SessionID& window_id,
                                 ExpectedErrorResult expected_result = {});

  MultiStep CloseWindowAction(actor::TaskId& task_id,
                              SessionID& window_id,
                              ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorControllerWindowManagementUiTest::CreateWindowAction(
    actor::TaskId& task_id,
    ExpectedErrorResult expected_result) {
  auto create_window_provider = base::BindLambdaForTesting([&task_id]() {
    optimization_guide::proto::Actions create_window =
        actor::MakeCreateWindow();
    create_window.set_task_id(task_id.value());
    return EncodeActionProto(create_window);
  });
  return ExecuteAction(std::move(create_window_provider),
                       std::move(expected_result));
}

MultiStep GlicActorControllerWindowManagementUiTest::ActivateWindowAction(
    actor::TaskId& task_id,
    SessionID& window_id,
    ExpectedErrorResult expected_result) {
  auto activate_window_provider =
      base::BindLambdaForTesting([&task_id, &window_id]() {
        optimization_guide::proto::Actions activate_window =
            actor::MakeActivateWindow(window_id);
        activate_window.set_task_id(task_id.value());
        return EncodeActionProto(activate_window);
      });
  return ExecuteAction(std::move(activate_window_provider),
                       std::move(expected_result));
}

MultiStep GlicActorControllerWindowManagementUiTest::CloseWindowAction(
    actor::TaskId& task_id,
    SessionID& window_id,
    ExpectedErrorResult expected_result) {
  auto close_window_provider =
      base::BindLambdaForTesting([&task_id, &window_id]() {
        optimization_guide::proto::Actions close_window =
            actor::MakeCloseWindow(window_id);
        close_window.set_task_id(task_id.value());
        return EncodeActionProto(close_window);
      });
  return ExecuteAction(std::move(close_window_provider),
                       std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerWindowManagementUiTest,
                       WindowManagementTools) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP() << "Programmatic window activation doesn't work on wayland and"
                 << "this test checks window activation.";
  }
#endif

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  size_t initial_window_count = 0;
  Browser* initial_window = browser();
  SessionID initial_window_session_id = SessionID::InvalidValue();

  Browser* created_window = nullptr;
  SessionID created_window_session_id = SessionID::InvalidValue();

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      WaitForWebContentsReady(kNewActorTabId, task_url),
      GetPageContextFromFocusedTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      Do([&]() {
        initial_window = BrowserList::GetInstance()->GetLastActive();
        initial_window_session_id = initial_window->session_id();
        initial_window_count = BrowserList::GetInstance()->size();
      }),

      // Create a new window
      CreateWindowAction(task_id_),
      Check([&]() {
              return BrowserList::GetInstance()->size() ==
                  initial_window_count + 1;
          },
          "New window was created"),
      CheckResult([]() { return BrowserList::GetInstance()->GetLastActive(); },
          testing::Ne(initial_window),
          "Last active window was changed"),

      Do([&]() {
        created_window = BrowserList::GetInstance()->GetLastActive();
        created_window_session_id = created_window->session_id();
      }),
      Check([&]() { return created_window->IsActive(); },
          "New window is active"),
      Check([&]() { return !initial_window->IsActive(); },
          "Initial window is inactive"),

      // Activate the initial window
      ActivateWindowAction(task_id_, initial_window_session_id),
      CheckResult(
          []() { return BrowserList::GetInstance()->GetLastActive(); },
          initial_window,
          "Initial window becomes last actived"),
      Check([&]() { return initial_window->IsActive(); },
          "Initial window is active"),
      Check([&]() { return !created_window->IsActive(); },
          "New window is inactive"),

      // Close the new window
      CloseWindowAction(task_id_, created_window_session_id),
      Check([&]() {
              return BrowserList::GetInstance()->size() == initial_window_count;
          },
          "Created window was closed"),
      CheckResult(
          []() { return BrowserList::GetInstance()->GetLastActive(); },
          initial_window,
          "Initial window remains active")
  );
  // clang-format on
}

}  //  namespace

}  // namespace glic::test
