// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/actor_task_holder.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/actor/core/task_source_info.h"
#include "components/critical_actions/core/browser/critical_action_service.h"

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

  task_id_ = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc, session_id_),
      ttc_service->enterprise_policy_checker(), /*options=*/nullptr,
      task_delegate_.GetWeakPtr(), &ttc_service->actor_ui_state_manager(),
      /*initial_invocation_source=*/std::nullopt,
      actor::ActorKeyedService::AllowedSchemes::kRequireHttpsOrHttpOrNtp);
  CHECK(!task_id_.is_null());

  const std::string task_id_str = base::NumberToString(task_id_.value());
  session_task_ids_.push_back(task_id_str);
  if (session_id_.has_value()) {
    if (auto* critical_action_service =
            critical_actions::CriticalActionFactory::GetForProfile(
                GetProfile())) {
      critical_action_service->SetCriticalActionsConversationId({task_id_str},
                                                                *session_id_);
    }
  }

  if (is_replacement) {
    session_controller_->GetJournal().SetTaskId(task_id_);
  }

  return task_id_;
}

void ActorTaskHolder::SetSessionId(const std::string& session_id) {
  if (session_id.empty()) {
    return;
  }
  session_id_ = session_id;

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  CHECK(actor_service);
  actor::ActorTask* task = actor_service->GetTask(task_id_);
  if (!task) {
    EnsureTaskCreated();
  } else {
    task->SetSourceId(session_id);
  }

  if (auto* critical_action_service =
          critical_actions::CriticalActionFactory::GetForProfile(
              GetProfile())) {
    critical_action_service->SetCriticalActionsConversationId(session_task_ids_,
                                                              session_id);
  }
}

Profile* ActorTaskHolder::GetProfile() {
  return session_controller_->GetProfile();
}

}  // namespace ttc
