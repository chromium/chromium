// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/autoclick.mojom.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "services/accessibility/public/mojom/automation_client.mojom.h"
#include "services/accessibility/public/mojom/file_loader.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ash {

// A fake Chrome OS Accessibility service to use for Chrome testing.
// This class acts as an AccessibilityServiceRouter in the browser process
// and then implements service mojom to act as a mock service.
class FakeAccessibilityService
    : public ax::AccessibilityServiceRouter,
      public ax::mojom::Automation,
      public ax::mojom::AssistiveTechnologyController,
      public ax::mojom::Autoclick {
 public:
  FakeAccessibilityService();
  FakeAccessibilityService(const FakeAccessibilityService&) = delete;
  FakeAccessibilityService& operator=(const FakeAccessibilityService&) = delete;
  ~FakeAccessibilityService() override;

  // AccessibilityServiceRouter:
  void BindAccessibilityServiceClient(
      mojo::PendingRemote<ax::mojom::AccessibilityServiceClient>
          accessibility_service_client) override;
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features)
      override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      ax::mojom::AssistiveTechnologyType type) override;

  // ax::mojom::Automation:
  void DispatchTreeDestroyedEvent(const ui::AXTreeID& tree_id) override;
  void DispatchActionResult(const ui::AXActionData& data, bool result) override;
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      int node_id,
      const ui::AXRelativeBounds& bounds) override;
  void DispatchGetTextLocationResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& rect) override;

  // ax::mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(
      const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features)
      override;

  // In the service, V8 JS implements autoclick.
  // ax::mojom::Autoclick:
  void RequestScrollableBoundsForPoint(const gfx::Point& point) override;

  //
  // Methods for testing.
  //

  // Whether the service client remote is bound.
  bool IsBound() const;

  // Waits for EnableAssistiveTechnology to be called |count| times.
  void WaitForATChangeCount(int count);

  // Gets the currently enabled assistive technology types.
  const std::set<ax::mojom::AssistiveTechnologyType>& GetEnabledATs() const {
    return enabled_ATs_;
  }

  int GetDevtoolsConnectionCount(ax::mojom::AssistiveTechnologyType type) const;

  // Allows tests to bind APIs multiple times, mimicking multiple
  // V8 instances in the service.
  void BindAnotherAutomation();
  void BindAnotherAutomationClient();
  void BindAnotherAutoclickClient();
  void BindAnotherSpeechRecognition();
  void BindAnotherTts();
  void BindAnotherUserInput();
  void BindAnotherUserInterface();

  //
  // Methods to pretend an AutomationClient request came from the service.
  //

  // Calls ax::mojom::AutomationClient::Enable or ::Disable.
  void AutomationClientEnable(bool enabled);

  // Waits for Automation events to come in.
  void WaitForAutomationEvents();

  //
  // Methods to pretend a SpeechRecognition request came from the service.
  //

  void RequestSpeechRecognitionStart(
      ax::mojom::StartOptionsPtr options,
      base::OnceCallback<void(ax::mojom::SpeechRecognitionStartInfoPtr)>
          callback);
  void RequestSpeechRecognitionStop(
      ax::mojom::StopOptionsPtr options,
      base::OnceCallback<void(const std::optional<std::string>&)> callback);

  //
  // Methods to pretend a TTS request came from the service.
  //

  // Sends a request for speech using the default options, but with on_event
  // set to true.
  void RequestSpeak(
      const std::string& utterance,
      base::OnceCallback<void(ax::mojom::TtsSpeakResultPtr)> callback);

  // Sends a request for speech using the given options.
  void RequestSpeak(
      const std::string& utterance,
      ax::mojom::TtsOptionsPtr options,
      base::OnceCallback<void(ax::mojom::TtsSpeakResultPtr)> callback);

  // Sends a request to stop speech.
  void RequestStop();

  // Sends a request to pause speech.
  void RequestPause();

  // Sends a request to resume speech.
  void RequestResume();

  // Asks if speech is in progress.
  void IsTtsSpeaking(base::OnceCallback<void(bool)> callback);

  // Sends a request from the service for the TTS voices list.
  void RequestTtsVoices(ax::mojom::Tts::GetVoicesCallback callback);

  //
  // Methods to pretend a UserInput request came from the service.
  //

  // Sends a request to send a synthetic key event.
  void RequestSendSyntheticKeyEventForShortcutOrNavigation(
      ax::mojom::SyntheticKeyEventPtr key_event);

  // Sends a request to send a synthetic mouse event.
  void RequestSendSyntheticMouseEvent(
      ax::mojom::SyntheticMouseEventPtr mouse_event);

  //
  // Methods to pretend a UserInterface request came from the service.
  //

  void RequestDarkenScreen(bool darken);

  void RequestOpenSettingsSubpage(const std::string& subpage);

  void RequestShowConfirmationDialog(
      const std::string& title,
      const std::string& description,
      const std::optional<std::string>& cancel_name,
      ax::mojom::UserInterface::ShowConfirmationDialogCallback callback);

  void RequestSetFocusRings(
      std::vector<ax::mojom::FocusRingInfoPtr> focus_rings,
      ax::mojom::AssistiveTechnologyType at_type);

  void RequestSetHighlights(const std::vector<gfx::Rect>& rects, SkColor color);

  void RequestSetVirtualKeyboardVisible(bool is_visible);

  // Getters for automation events.
  std::vector<ui::AXTreeID> tree_destroyed_events() const {
    return tree_destroyed_events_;
  }
  std::vector<std::tuple<ui::AXActionData, bool>> action_results() const {
    return action_results_;
  }
  std::vector<ui::AXTreeID> accessibility_events() const {
    return accessibility_events_;
  }
  std::vector<ui::AXTreeID> location_changes() const {
    return location_changes_;
  }

  // Loads a file indicated by |relative_path|, which must be a relative path.
  // The base path used to load paths from is the base accessibility resources
  // directory in ChromeOS. |callback| runs when the operation is completed.
  void RequestLoadFile(
      base::FilePath relative_path,
      ax::mojom::AccessibilityFileLoader::LoadCallback callback);

  void set_autoclick_scrollable_bounds(const gfx::Rect& bounds) {
    autoclick_scrollable_bounds_ = bounds;
  }

 private:
  // Emulates V8 getting the autoclick receiver in the service process.
  void OnAutoclickBoundCallback(
      mojo::PendingReceiver<ax::mojom::Autoclick> autoclick_receiver);

  base::OnceClosure change_ATs_closure_;
  std::set<ax::mojom::AssistiveTechnologyType> enabled_ATs_;
  base::OnceClosure automation_events_closure_;

  std::vector<ui::AXTreeID> tree_destroyed_events_;
  std::vector<std::tuple<ui::AXActionData, bool>> action_results_;
  std::vector<ui::AXTreeID> accessibility_events_;
  std::vector<ui::AXTreeID> location_changes_;

  gfx::Rect autoclick_scrollable_bounds_;

  std::map<ax::mojom::AssistiveTechnologyType, int> connect_devtools_counts;

  // Number of times ATs changed state.
  int at_change_count_ = 0;
  int expected_count_ = 0;

  mojo::AssociatedReceiverSet<ax::mojom::Automation> automation_receivers_;
  mojo::RemoteSet<ax::mojom::AutomationClient> automation_client_remotes_;

  mojo::RemoteSet<ax::mojom::SpeechRecognition> sr_remotes_;
  mojo::RemoteSet<ax::mojom::AutoclickClient> autoclick_client_remotes_;
  mojo::ReceiverSet<ax::mojom::Autoclick> autoclick_receivers_;
  mojo::RemoteSet<ax::mojom::Tts> tts_remotes_;
  mojo::RemoteSet<ax::mojom::UserInput> ui_remotes_;
  mojo::RemoteSet<ax::mojom::UserInterface> ux_remotes_;

  mojo::ReceiverSet<ax::mojom::AssistiveTechnologyController>
      at_controller_receivers_;
  mojo::Remote<ax::mojom::AccessibilityServiceClient>
      accessibility_service_client_remote_;
  mojo::Remote<ax::mojom::AccessibilityFileLoader> file_loader_remote_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_
