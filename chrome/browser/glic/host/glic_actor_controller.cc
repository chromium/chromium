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
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tab_collections/public/tab_interface.h"

namespace glic {

namespace {

void OnGetContextFromFocusedTab(
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    mojom::GetContextResultPtr tab_context_result) {
  if (tab_context_result->is_error_reason()) {
    mojom::ActInFocusedTabResultPtr result =
        mojom::ActInFocusedTabResult::NewErrorReason(
            mojom::ActInFocusedTabErrorReason::kGetContextFailed);
    UMA_HISTOGRAM_ENUMERATION("Glic.Action.ActInFocusedTabErrorReason",
                              result->get_error_reason());
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

GlicActorController::GlicActorController() = default;

GlicActorController::~GlicActorController() = default;

void GlicActorController::Act(
    FocusedTabData focused_tab_data,
    const optimization_guide::proto::BrowserAction& action,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback) {
  // TODO(https://crbug.com/402235832): Check that the tab is valid for action,
  // and get the TabInterface from the `focused_tab_data`.

  // TODO(https://crbug.com/402086398): Initialize the controller when the task
  // is started.
  if (!actor_coordinator_) {
    actor_coordinator_ = std::make_unique<actor::ActorCoordinator>();
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(focused_tab_data.focus());
  CHECK(tab);

  actor_coordinator_->Act(
      *tab, action,
      base::BindOnce(&GlicActorController::OnActionFinished, GetWeakPtr(),
                     focused_tab_data, options, std::move(callback)));
}

void GlicActorController::OnActionFinished(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    mojom::WebClientHandler::ActInFocusedTabCallback callback,
    bool action_succeeded) const {
  if (!action_succeeded) {
    mojom::ActInFocusedTabResultPtr result =
        mojom::ActInFocusedTabResult::NewErrorReason(
            mojom::ActInFocusedTabErrorReason::kInvalidActionProto);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

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

  auto fetcher = std::make_unique<glic::GlicPageContextFetcher>();
  fetcher->Fetch(
      focused_tab_data, options,
      base::BindOnce(
          // Bind `fetcher` to the callback to keep it in scope until it
          // returns.
          [](std::unique_ptr<glic::GlicPageContextFetcher> fetcher,
             mojom::WebClientHandler::GetContextFromFocusedTabCallback callback,
             mojom::GetContextResultPtr result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(fetcher), std::move(callback)));
}

}  // namespace glic
