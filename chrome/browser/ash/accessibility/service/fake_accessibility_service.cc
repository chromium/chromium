// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"

#include <tuple>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"

namespace ash {

FakeAccessibilityService::FakeAccessibilityService() = default;
FakeAccessibilityService::~FakeAccessibilityService() = default;

void FakeAccessibilityService::BindAccessibilityServiceClient(
    mojo::PendingRemote<ax::mojom::AccessibilityServiceClient>
        accessibility_service_client) {
  accessibility_service_client_remote_.Bind(
      std::move(accessibility_service_client));
}

void FakeAccessibilityService::BindAnotherAutomation() {
  mojo::PendingRemote<ax::mojom::Automation> automation_remote;
  automation_receivers_.Add(this,
                            automation_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client_receiver;
  automation_client_remotes_.Add(
      automation_client_receiver.InitWithNewPipeAndPassRemote());

  accessibility_service_client_remote_->BindAutomation(
      std::move(automation_remote), std::move(automation_client_receiver));
}

void FakeAccessibilityService::BindAnotherTts() {
  mojo::PendingReceiver<ax::mojom::Tts> tts_receiver;
  tts_remotes_.Add(tts_receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindTts(std::move(tts_receiver));
}

void FakeAccessibilityService::BindAssistiveTechnologyController(
    mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
        at_controller_receiver,
    const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_receivers_.Add(this, std::move(at_controller_receiver));
  EnableAssistiveTechnology(enabled_features);
}

void FakeAccessibilityService::DispatchTreeDestroyedEvent(
    const ui::AXTreeID& tree_id) {
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
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  accessibility_events_.emplace_back(tree_id);
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::DispatchAccessibilityLocationChange(
    const ui::AXTreeID& tree_id,
    int node_id,
    const ui::AXRelativeBounds& bounds) {
  location_changes_.emplace_back(tree_id);
  if (automation_events_closure_)
    std::move(automation_events_closure_).Run();
}

void FakeAccessibilityService::EnableAssistiveTechnology(
    const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features) {
  enabled_ATs_ = std::set(enabled_features.begin(), enabled_features.end());
  if (change_ATs_closure_)
    std::move(change_ATs_closure_).Run();
}

void FakeAccessibilityService::WaitForATChanged() {
  base::RunLoop runner;
  change_ATs_closure_ = runner.QuitClosure();
  runner.Run();
}

bool FakeAccessibilityService::IsBound() const {
  return accessibility_service_client_remote_.is_bound();
}

void FakeAccessibilityService::AutomationClientEnable(bool enabled) {
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

void FakeAccessibilityService::RequestSpeak(
    const std::string& utterance,
    base::OnceCallback<void(ax::mojom::TtsSpeakResultPtr)> callback) {
  auto options = ax::mojom::TtsOptions::New();
  options->on_event = true;
  RequestSpeak(utterance, std::move(options), std::move(callback));
}

void FakeAccessibilityService::RequestSpeak(
    const std::string& utterance,
    ax::mojom::TtsOptionsPtr options,
    base::OnceCallback<void(ax::mojom::TtsSpeakResultPtr)> callback) {
  CHECK_EQ(tts_remotes_.size(), 1u);
  for (auto& tts_client : tts_remotes_) {
    tts_client->Speak(utterance, std::move(options), std::move(callback));
  }
}

void FakeAccessibilityService::RequestStop() {
  for (auto& tts_client : tts_remotes_) {
    tts_client->Stop();
  }
}

void FakeAccessibilityService::RequestPause() {
  for (auto& tts_client : tts_remotes_) {
    tts_client->Pause();
  }
}

void FakeAccessibilityService::RequestResume() {
  for (auto& tts_client : tts_remotes_) {
    tts_client->Resume();
  }
}

void FakeAccessibilityService::IsTtsSpeaking(
    base::OnceCallback<void(bool)> callback) {
  CHECK_EQ(tts_remotes_.size(), 1u);
  for (auto& tts_client : tts_remotes_) {
    tts_client->IsSpeaking(std::move(callback));
  }
}

void FakeAccessibilityService::RequestTtsVoices(
    ax::mojom::Tts::GetVoicesCallback callback) {
  CHECK_EQ(tts_remotes_.size(), 1u);
  for (auto& tts_client : tts_remotes_) {
    tts_client->GetVoices(std::move(callback));
  }
}

}  // namespace ash
