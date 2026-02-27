// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_interactive_browser_test.h"

#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/common/chrome_switches.h"

using actor::ExpectOkResult;
using actor::TaskId;
using base::test::TestFuture;

ActorUiInteractiveBrowserTest::ActorUiInteractiveBrowserTest() = default;

ActorUiInteractiveBrowserTest::~ActorUiInteractiveBrowserTest() = default;

void ActorUiInteractiveBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  InteractiveBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kGlicDev);
  // Skips FRE experience.
  command_line->AppendSwitch(switches::kGlicAutomation);
}

void ActorUiInteractiveBrowserTest::StartActingOnTab() {
  task_id_ =
      actor_keyed_service()->CreateTask(actor::NoEnterprisePolicyChecker());
  TestFuture<actor::mojom::ActionResultPtr> future;
  actor_keyed_service()->GetTask(task_id_)->AddTab(
      browser()->GetActiveTabInterface()->GetHandle(), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ExpectOkResult(future);
  actor::PerformActionsFuture result_future;
  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(actor::MakeWaitRequest());
  actor_keyed_service()->PerformActions(task_id_, std::move(actions),
                                        actor::ActorTaskMetadata(),
                                        result_future.GetCallback());
  ASSERT_TRUE(result_future.Wait());
  ExpectOkResult(result_future);
}

void ActorUiInteractiveBrowserTest::PauseTask() {
  actor_keyed_service()->GetTask(task_id_)->Pause(/*from_actor=*/true);
}

void ActorUiInteractiveBrowserTest::CompleteTask() {
  actor_keyed_service()->StopTask(
      task_id_, actor::ActorTask::StoppedReason::kTaskComplete);
}
