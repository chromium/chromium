// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"

#include <tuple>

#include "base/run_loop.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ash {

FakeAccessibilityService::FakeAccessibilityService() = default;
FakeAccessibilityService::~FakeAccessibilityService() = default;

void FakeAccessibilityService::BindAutomationWithClient(
    mojo::PendingRemote<ax::mojom::AutomationClient>
        accessibility_client_remote,
    mojo::PendingReceiver<ax::mojom::Automation> automation_receiver) {
  automation_client_remotes_.Add(std::move(accessibility_client_remote));
  automation_receivers_.Add(this, std::move(automation_receiver));
}

void FakeAccessibilityService::BindAssistiveTechnologyController(
    mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
        at_controller_receiver,
    const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_receivers_.Add(this, std::move(at_controller_receiver));
  for (auto feature : enabled_features) {
    EnableAssistiveTechnology(feature, /*enabled=*/true);
  }
}

void FakeAccessibilityService::DispatchTreeDestroyedEvent(
    const base::UnguessableToken& tree_id) {
  tree_destroyed_events_.emplace_back(tree_id);
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::DispatchActionResult(
    const ui::AXActionData& data,
    bool result) {
  action_results_.emplace_back(std::make_tuple(data, result));
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::DispatchAccessibilityEvents(
    const base::UnguessableToken& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::DispatchAccessibilityLocationChange(
    const base::UnguessableToken& tree_id,
    int node_id,
    const ui::AXRelativeBounds& bounds) {
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::EnableAssistiveTechnology(
    ax::mojom::AssistiveTechnologyType type,
    bool enabled) {
  if (enabled)
    enabled_ATs_.insert(type);
  else
    enabled_ATs_.erase(type);

  if (change_ATs_closure_)
    std::move(change_ATs_closure_).Run();
}

void FakeAccessibilityService::WaitForATChanged() {
  base::RunLoop runner;
  change_ATs_closure_ = runner.QuitClosure();
  runner.Run();
}

bool FakeAccessibilityService::IsBound() {
  return automation_client_remotes_.size() > 0 &&
         automation_client_remotes_.begin()->is_bound();
}

void FakeAccessibilityService::EnableAutomationClient(bool enabled) {
  // TODO(crbug.com/1355633): Add once AutomationClient mojom is added.
  // for (auto& automation_client : automation_client_remotes_) {
  //   enabled ? automation_client->Enable() : automation_client->Disable();
  // }
}

void FakeAccessibilityService::WaitForAutomationEvents() {
  base::RunLoop runner;
  automation_events_closure_ = runner.QuitClosure();
  runner.Run();
}

}  // namespace ash
