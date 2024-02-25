// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/autoclick.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"

namespace ash {

FakeAccessibilityService::FakeAccessibilityService() = default;
FakeAccessibilityService::~FakeAccessibilityService() = default;

void FakeAccessibilityService::BindAccessibilityServiceClient(
    mojo::PendingRemote<ax::mojom::AccessibilityServiceClient>
        accessibility_service_client) {
  accessibility_service_client_remote_.Bind(
      std::move(accessibility_service_client));
  accessibility_service_client_remote_->BindAccessibilityFileLoader(
      file_loader_remote_.BindNewPipeAndPassReceiver());
}

void FakeAccessibilityService::BindAnotherAutoclickClient() {
  mojo::PendingReceiver<ax::mojom::AutoclickClient> autoclick_client_receiver;
  autoclick_client_remotes_.Add(
      autoclick_client_receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindAutoclickClient(
      std::move(autoclick_client_receiver));

  // Now connect the autoclick remote in the service back to the client in the
  // browser by getting a PendingReceiver<Autoclick> from the browser.
  for (auto& remote : autoclick_client_remotes_) {
    remote->BindAutoclick(
        base::BindOnce(&FakeAccessibilityService::OnAutoclickBoundCallback,
                       base::Unretained(this)));
  }
}

void FakeAccessibilityService::BindAnotherAutomation() {
  mojo::PendingAssociatedRemote<ax::mojom::Automation> automation_remote;
  automation_receivers_.Add(
      this, automation_remote.InitWithNewEndpointAndPassReceiver());
  accessibility_service_client_remote_->BindAutomation(
      std::move(automation_remote));
}

void FakeAccessibilityService::BindAnotherAutomationClient() {
  mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client_receiver;
  automation_client_remotes_.Add(
      automation_client_receiver.InitWithNewPipeAndPassRemote());
}

void FakeAccessibilityService::BindAnotherSpeechRecognition() {
  mojo::PendingReceiver<ax::mojom::SpeechRecognition> receiver;
  sr_remotes_.Add(receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindSpeechRecognition(
      std::move(receiver));
}

void FakeAccessibilityService::BindAnotherTts() {
  mojo::PendingReceiver<ax::mojom::Tts> tts_receiver;
  tts_remotes_.Add(tts_receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindTts(std::move(tts_receiver));
}

void FakeAccessibilityService::BindAnotherUserInput() {
  mojo::PendingReceiver<ax::mojom::UserInput> ui_receiver;
  ui_remotes_.Add(ui_receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindUserInput(std::move(ui_receiver));
}

void FakeAccessibilityService::BindAnotherUserInterface() {
  mojo::PendingReceiver<ax::mojom::UserInterface> ux_receiver;
  ux_remotes_.Add(ux_receiver.InitWithNewPipeAndPassRemote());
  accessibility_service_client_remote_->BindUserInterface(
      std::move(ux_receiver));
}

void FakeAccessibilityService::BindAssistiveTechnologyController(
    mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
        at_controller_receiver,
    const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_receivers_.Add(this, std::move(at_controller_receiver));
  EnableAssistiveTechnology(enabled_features);
}

void FakeAccessibilityService::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    ax::mojom::AssistiveTechnologyType type) {
  auto it = connect_devtools_counts.find(type);
  if (it == connect_devtools_counts.end()) {
    connect_devtools_counts[type] = 0;
  }
  connect_devtools_counts[type]++;
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

void FakeAccessibilityService::DispatchGetTextLocationResult(
    const ui::AXActionData& data,
    const std::optional<gfx::Rect>& rect) {}

void FakeAccessibilityService::EnableAssistiveTechnology(
    const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features) {
  enabled_ATs_ = std::set(enabled_features.begin(), enabled_features.end());
  at_change_count_++;
  if (change_ATs_closure_ && at_change_count_ == expected_count_) {
    expected_count_ = 0;
    std::move(change_ATs_closure_).Run();
  }
}

void FakeAccessibilityService::RequestScrollableBoundsForPoint(
    const gfx::Point& point) {
  for (auto& remote : autoclick_client_remotes_) {
    remote->HandleScrollableBoundsForPointFound(autoclick_scrollable_bounds_);
  }
}

void FakeAccessibilityService::WaitForATChangeCount(int count) {
  if (count == at_change_count_) {
    return;
  }
  expected_count_ = count;
  base::RunLoop runner;
  change_ATs_closure_ = runner.QuitClosure();
  runner.Run();
}

int FakeAccessibilityService::GetDevtoolsConnectionCount(
    ax::mojom::AssistiveTechnologyType type) const {
  auto it = connect_devtools_counts.find(type);
  if (it == connect_devtools_counts.end()) {
    return 0;
  }
  return it->second;
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

void FakeAccessibilityService::RequestSpeechRecognitionStart(
    ax::mojom::StartOptionsPtr options,
    base::OnceCallback<void(ax::mojom::SpeechRecognitionStartInfoPtr)>
        callback) {
  CHECK_EQ(sr_remotes_.size(), 1u);
  for (auto& remote : sr_remotes_) {
    remote->Start(std::move(options), std::move(callback));
  }
}

void FakeAccessibilityService::RequestSpeechRecognitionStop(
    ax::mojom::StopOptionsPtr options,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  CHECK_EQ(sr_remotes_.size(), 1u);
  for (auto& remote : sr_remotes_) {
    remote->Stop(std::move(options), std::move(callback));
  }
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

void FakeAccessibilityService::
    RequestSendSyntheticKeyEventForShortcutOrNavigation(
        ax::mojom::SyntheticKeyEventPtr key_event) {
  for (auto& ui_client : ui_remotes_) {
    ui_client->SendSyntheticKeyEventForShortcutOrNavigation(
        mojo::Clone(key_event));
  }
}

void FakeAccessibilityService::RequestSendSyntheticMouseEvent(
    ax::mojom::SyntheticMouseEventPtr mouse_event) {
  for (auto& ui_client : ui_remotes_) {
    ui_client->SendSyntheticMouseEvent(mojo::Clone(mouse_event));
  }
}

void FakeAccessibilityService::RequestDarkenScreen(bool darken) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->DarkenScreen(darken);
  }
}

void FakeAccessibilityService::RequestOpenSettingsSubpage(
    const std::string& subpage) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->OpenSettingsSubpage(subpage);
  }
}

void FakeAccessibilityService::RequestShowConfirmationDialog(
    const std::string& title,
    const std::string& description,
    const std::optional<std::string>& cancel_name,
    ax::mojom::UserInterface::ShowConfirmationDialogCallback callback) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->ShowConfirmationDialog(title, description, cancel_name,
                                      std::move(callback));
  }
}

void FakeAccessibilityService::RequestSetFocusRings(
    std::vector<ax::mojom::FocusRingInfoPtr> focus_rings,
    ax::mojom::AssistiveTechnologyType at_type) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->SetFocusRings(mojo::Clone(focus_rings), at_type);
  }
}

void FakeAccessibilityService::RequestSetHighlights(
    const std::vector<gfx::Rect>& rects,
    SkColor color) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->SetHighlights(rects, color);
  }
}

void FakeAccessibilityService::RequestSetVirtualKeyboardVisible(
    bool is_visible) {
  for (auto& ux_client : ux_remotes_) {
    ux_client->SetVirtualKeyboardVisible(is_visible);
  }
}

void FakeAccessibilityService::RequestLoadFile(
    base::FilePath relative_path,
    ax::mojom::AccessibilityFileLoader::LoadCallback callback) {
  file_loader_remote_->Load(relative_path, std::move(callback));
}

void FakeAccessibilityService::OnAutoclickBoundCallback(
    mojo::PendingReceiver<ax::mojom::Autoclick> autoclick_receiver) {
  autoclick_receivers_.Add(this, std::move(autoclick_receiver));
}

}  // namespace ash
