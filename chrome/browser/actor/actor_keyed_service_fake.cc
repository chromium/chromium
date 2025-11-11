// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_fake.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mocks/mock_event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"

namespace actor {
using ::testing::_;

ActorKeyedServiceFake::ActorKeyedServiceFake(Profile* profile)
    : ActorKeyedService(profile) {}

ActorKeyedServiceFake::~ActorKeyedServiceFake() = default;

TaskId ActorKeyedServiceFake::CreateTaskForTesting() {
  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher =
      ui::NewMockUiEventDispatcher();
  std::unique_ptr<ui::UiEventDispatcher> task_ui_event_dispatcher =
      ui::NewMockUiEventDispatcher();

  auto* mock_ui_dispatcher =
      static_cast<ui::MockUiEventDispatcher*>(ui_event_dispatcher.get());
  auto* mock_task_ui_dispatcher =
      static_cast<ui::MockUiEventDispatcher*>(task_ui_event_dispatcher.get());

  for (auto& mock : {mock_ui_dispatcher, mock_task_ui_dispatcher}) {
    ON_CALL(*mock, OnPreTool(_, _))
        .WillByDefault(
            UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                MakeOkResult, /*requires_page_stabilization=*/true)));
    ON_CALL(*mock, OnPostTool(_, _))
        .WillByDefault(
            UiEventDispatcherCallback<ToolRequest>(base::BindRepeating(
                MakeOkResult, /*requires_page_stabilization=*/true)));
    ON_CALL(*mock, OnActorTaskAsyncChange(_, _))
        .WillByDefault(UiEventDispatcherCallback<
                       ui::UiEventDispatcher::ActorTaskAsyncChange>(
            base::BindRepeating(MakeOkResult,
                                /*requires_page_stabilization=*/true)));
  }
  auto execution_engine = ExecutionEngine::CreateForTesting(
      GetProfile(), std::move(ui_event_dispatcher));
  auto actor_task =
      std::make_unique<ActorTask>(GetProfile(), std::move(execution_engine),
                                  std::move(task_ui_event_dispatcher));
  return AddActiveTask(std::move(actor_task));
}

}  // namespace actor
