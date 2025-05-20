// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

using optimization_guide::proto::ActionInformation;

mojom::ActInFocusedTabResultPtr MakeActErrorResult(
    mojom::ActInFocusedTabErrorReason error_reason) {
  mojom::ActInFocusedTabResultPtr result =
      mojom::ActInFocusedTabResult::NewErrorReason(error_reason);
  UMA_HISTOGRAM_ENUMERATION("Glic.Action.ActInFocusedTabErrorReason",
                            result->get_error_reason());
  return result;
}

void PostTaskForActCallback(
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    mojom::ActInFocusedTabErrorReason error_reason) {
  mojom::ActInFocusedTabResultPtr result = MakeActErrorResult(error_reason);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  return;
}

void OnGetContextFromFocusedTab(
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    mojom::GetContextResultPtr tab_context_result) {
  if (tab_context_result->is_error_reason()) {
    mojom::ActInFocusedTabResultPtr result = MakeActErrorResult(
        mojom::ActInFocusedTabErrorReason::kGetContextFailed);
    std::move(callback).Run(std::move(result));
    return;
  }

  mojom::ActInFocusedTabResultPtr result =
      mojom::ActInFocusedTabResult::NewActInFocusedTabResponse(
          mojom::ActInFocusedTabResponse::New(
              std::move(tab_context_result->get_tab_context())));

  std::move(callback).Run(std::move(result));
}

}  // namespace

GlicActorController::GlicActorController(Profile* profile) : profile_(profile) {
  CHECK(profile_);
  actor::ActorCoordinator::RegisterWithProfile(profile_);
}

GlicActorController::~GlicActorController() = default;

void GlicActorController::Act(
    FocusedTabData focused_tab_data,
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) {
  if (!actor_coordinator_) {
    actor_coordinator_ = std::make_unique<actor::ActorCoordinator>(profile_);
  }

  if (!actor_coordinator_->HasTask()) {
    // Start of a new task, which must begin with a navigate action.
    // The OnTaskStarted callback will perform the action after the task is
    // setup by the coordinator.
    // TODO(https://crbug.com/407860715): Implement a separate host API to start
    // a task, and remove action handling here.
    actor_coordinator_->StartTask(
        action,
        base::BindOnce(&GlicActorController::OnTaskStarted, GetWeakPtr(),
                       action, options, std::move(callback)));
    return;
  }

  ActImpl(focused_tab_data, action, options, std::move(callback));
}

void GlicActorController::StopTask() {
  if (!actor_coordinator_) {
    return;
  }
  actor_coordinator_->StopTask();
}

bool GlicActorController::IsActorCoordinatorActingOnTab(
    const content::WebContents* tab) const {
  return actor_coordinator_ && actor_coordinator_->HasTaskForTab(tab);
}

actor::ActorCoordinator& GlicActorController::GetActorCoordinatorForTesting() {
  if (!actor_coordinator_) {
    actor_coordinator_ = std::make_unique<actor::ActorCoordinator>(profile_);
  }
  return *actor_coordinator_;
}

void GlicActorController::OnTaskStarted(
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback act_callback,
    base::WeakPtr<tabs::TabInterface> tab) const {
  if (!tab) {
    PostTaskForActCallback(std::move(act_callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
    return;
  }
  // TODO(https://crbug.com/402086398): Check that the tab is valid for action,
  // maybe reusing the validation in GlicFocusedTabManager.
  FocusedTabData focused_tab_data{tab->GetContents()->GetWeakPtr()};

  ActImpl(focused_tab_data, action, options, std::move(act_callback));
}

void GlicActorController::ActImpl(
    FocusedTabData focused_tab_data,
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) const {
  actor_coordinator_->Act(
      action,
      base::BindOnce(&GlicActorController::OnActionFinished, GetWeakPtr(),
                     focused_tab_data, options, std::move(callback)));
}

void GlicActorController::OnActionFinished(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    actor::mojom::ActionResultPtr result) const {
  if (!actor::IsOk(*result)) {
    PostTaskForActCallback(std::move(callback),
                           mojom::ActInFocusedTabErrorReason::kTargetNotFound);
    return;
  }

  // TODO(https://crbug.com/398271171): Remove when the actor coordinator
  // handles getting a new observation.
  GetContextFromFocusedTab(
      focused_tab_data, options,
      base::BindOnce(OnGetContextFromFocusedTab, std::move(callback)));
}

void GlicActorController::GetContextFromFocusedTab(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) const {
  // TODO(https://crbug.com/402086398): Figure out if/how this can be shared
  // with GlicKeyedService::GetContextFromFocusedTab(). It's not clear yet if
  // the same permission checks, etc. should apply here.

  FetchPageContext(focused_tab_data, options, std::move(callback));
}

base::WeakPtr<const GlicActorController> GlicActorController::GetWeakPtr()
    const {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<GlicActorController> GlicActorController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace glic
