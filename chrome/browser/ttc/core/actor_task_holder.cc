// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/actor_task_holder.h"

#include <optional>

#include "base/check.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal.h"
#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/actor/core/task_source_info.h"

namespace ttc {

ActorTaskHolder::ActorTaskHolder(SessionControllerImpl& session_controller)
    : session_controller_(session_controller) {
  // Start the task up front so that the whole session is journaled under it.
  EnsureTaskCreated();
}

ActorTaskHolder::~ActorTaskHolder() {
  if (!task_id_.is_null()) {
    auto* actor_service = actor::ActorKeyedService::Get(GetProfile());
    CHECK(actor_service);

    // Cancel a task that never acted so that it isn't surfaced to the user.
    // TODO(b/544821996): Revisit once TTC has its own task client and delegate.
    actor::ActorTask::StoppedReason stop_reason =
        actor::ActorTask::StoppedReason::kTaskComplete;
    if (const actor::ActorTask* task = actor_service->GetTask(task_id_);
        task && task->GetState() == actor::ActorTask::State::kCreated) {
      stop_reason = actor::ActorTask::StoppedReason::kStoppedByUser;
    }
    actor_service->StopTask(task_id_, stop_reason);
  }
}

actor::TaskId ActorTaskHolder::EnsureTaskCreated() {
  // TtcKeyedServiceFactory doesn't create TTC without the actor service.
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);

  if (!task_id_.is_null() && actor_service->GetTask(task_id_)) {
    return task_id_;
  }

  const bool is_replacement = !task_id_.is_null();

  // The session this object belongs to is owned by the TtcKeyedService, so it
  // is guaranteed to exist.
  TtcKeyedService* ttc_service = TtcKeyedService::Get(GetProfile());
  CHECK(ttc_service);

  // TODO(b/544821996): Provide an ActorTaskDelegate.
  task_id_ = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc, "ttc"),
      actor::GetNullEnterprisePolicyChecker(), /*options=*/nullptr,
      /*delegate=*/nullptr, &ttc_service->actor_ui_state_manager(),
      /*initial_invocation_source=*/std::nullopt,
      actor::ActorKeyedService::AllowedSchemes::kRequireHttpsOrHttpOrNtp);

  if (is_replacement) {
    session_controller_->GetJournal().SetTaskId(task_id_);
  }

  return task_id_;
}

Profile* ActorTaskHolder::GetProfile() {
  return session_controller_->GetProfile();
}

}  // namespace ttc
