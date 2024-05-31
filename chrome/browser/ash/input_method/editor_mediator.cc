// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include <optional>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_text_query_from_manta.h"
#include "chrome/browser/ash/input_method/editor_text_query_from_memory.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash::input_method {

EditorMediator::EditorMediator(
    Profile* profile,
    std::unique_ptr<EditorGeolocationProvider> editor_geolocation_provider)
    : profile_(profile),
      panel_manager_(this),
      editor_geolocation_provider_(std::move(editor_geolocation_provider)),
      editor_context_(this, this, editor_geolocation_provider_.get()),
      editor_switch_(
          std::make_unique<EditorSwitch>(this, profile, &editor_context_)),
      metrics_recorder_(
          std::make_unique<EditorMetricsRecorder>(&editor_context_,
                                                  GetEditorOpportunityMode())),
      consent_store_(
          std::make_unique<EditorConsentStore>(profile->GetPrefs(),
                                               metrics_recorder_.get())) {
  editor_context_.OnTabletModeUpdated(
      display::Screen::GetScreen()->InTabletMode());
}

EditorMediator::~EditorMediator() = default;

void EditorMediator::BindEditorClient(
    mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) {
  if (editor_client_connector_ != nullptr) {
    editor_client_connector_->BindEditorClient(std::move(pending_receiver));
  }
}

void EditorMediator::OnEditorServiceConnected(bool is_connection_successful) {}

void EditorMediator::SetUpNewEditorService() {
  if (editor_service_connector_.SetUpNewEditorService()) {
    mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>
        system_actuator_remote;
    mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
        text_query_provider_remote;
    mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
        editor_client_connector_receiver;
    mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
        editor_event_sink_receiver;

    system_actuator_ = std::make_unique<EditorSystemActuator>(
        profile_, system_actuator_remote.InitWithNewEndpointAndPassReceiver(),
        this);
    text_query_provider_ = std::make_unique<EditorTextQueryProvider>(
        text_query_provider_remote.InitWithNewEndpointAndPassReceiver(),
        metrics_recorder_.get(),
        std::make_unique<EditorTextQueryFromManta>(profile_));
    editor_client_connector_ = std::make_unique<EditorClientConnector>(
        editor_client_connector_receiver.InitWithNewEndpointAndPassRemote());
    editor_event_proxy_ = std::make_unique<EditorEventProxy>(
        editor_event_sink_receiver.InitWithNewEndpointAndPassRemote());

    editor_service_connector_.BindEditor(
        std::move(editor_client_connector_receiver),
        std::move(editor_event_sink_receiver),
        std::move(system_actuator_remote),
        std::move(text_query_provider_remote));

    // TODO: b:300838514 - We should only bind the native UI with the shared lib
    // when the Rewrite UI is shown. Consider add a listener to the
    // write/rewrite UI and move the binding there.
    panel_manager_.BindEditorClient();
  }
}

void EditorMediator::BindEditorPanelManager(
    mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
        pending_receiver) {
  panel_manager_.BindReceiver(std::move(pending_receiver));
}

void EditorMediator::OnContextUpdated() {
  editor_switch_->OnContextUpdated();
}

std::optional<ukm::SourceId> EditorMediator::GetUkmSourceId() {
  TextInputTarget* text_input = IMEBridge::Get()->GetInputContextHandler();
  if (!text_input) {
    return std::nullopt;
  }

  ukm::SourceId source_id = text_input->GetClientSourceForMetrics();
  if (source_id == ukm::kInvalidSourceId) {
    return std::nullopt;
  }
  return source_id;
}

void EditorMediator::OnFocus(int context_id) {
  if (mako_bubble_coordinator_.IsShowingUI() ||
      panel_manager_.IsEditorMenuVisible()) {
    return;
  }

  if (IsAllowedForUse() && !editor_service_connector_.IsBound()) {
    SetUpNewEditorService();
  }
  GetTextFieldContextualInfo(
      base::BindOnce(&EditorMediator::OnTextFieldContextualInfoChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  if (system_actuator_ != nullptr) {
    system_actuator_->OnFocus(context_id);
  }
}

void EditorMediator::OnBlur() {
  if (mako_bubble_coordinator_.IsShowingUI() ||
      panel_manager_.IsEditorMenuVisible()) {
    return;
  }
}

void EditorMediator::OnActivateIme(std::string_view engine_id) {
  editor_context_.OnActivateIme(engine_id);
}

void EditorMediator::OnDisplayTabletStateChanged(display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
      editor_context_.OnTabletModeUpdated(/*tablet_mode_enabled=*/false);
      break;
    case display::TabletState::kEnteringTabletMode:
      editor_context_.OnTabletModeUpdated(/*tablet_mode_enabled=*/true);
      if (mako_bubble_coordinator_.IsShowingUI()) {
        mako_bubble_coordinator_.CloseUI();
      }
      break;
    case display::TabletState::kInTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void EditorMediator::OnSurroundingTextChanged(const std::u16string& text,
                                              gfx::Range selection_range) {
  if (mako_bubble_coordinator_.IsShowingUI() ||
      panel_manager_.IsEditorMenuVisible()) {
    return;
  }

  surrounding_text_ = {.text = text, .selection_range = selection_range};
}

void EditorMediator::Announce(const std::u16string& message) {
  announcer_.Announce(message);
}

void EditorMediator::ProcessConsentAction(ConsentAction consent_action) {
  consent_store_->ProcessConsentAction(consent_action);
  HandleTrigger(/*preset_query_id=*/std::nullopt,
                /*freeform_text=*/std::nullopt);
}

void EditorMediator::ShowUI() {
  mako_bubble_coordinator_.ShowUI();
}

void EditorMediator::CloseUI() {
  mako_bubble_coordinator_.CloseUI();
}

size_t EditorMediator::GetSelectedTextLength() {
  return surrounding_text_.selection_range.length();
}

void EditorMediator::OnEditorModeChanged(const EditorMode& mode) {
  panel_manager_.NotifyEditorModeChanged(mode);
}

void EditorMediator::OnPromoCardDeclined() {
  consent_store_->ProcessPromoCardAction(PromoCardAction::kDeclined);
}

void EditorMediator::HandleTrigger(
    std::optional<std::string_view> preset_query_id,
    std::optional<std::string_view> freeform_text) {
  metrics_recorder_->SetTone(preset_query_id, freeform_text);
  switch (GetEditorMode()) {
    case EditorMode::kRewrite:
      mako_bubble_coordinator_.LoadEditorUI(profile_, MakoEditorMode::kRewrite,
                                            preset_query_id, freeform_text);
      metrics_recorder_->LogEditorState(EditorStates::kNativeRequest);
      break;
    case EditorMode::kWrite:
      mako_bubble_coordinator_.LoadEditorUI(profile_, MakoEditorMode::kWrite,
                                            preset_query_id, freeform_text);
      metrics_recorder_->LogEditorState(EditorStates::kNativeRequest);
      break;
    case EditorMode::kConsentNeeded:
      mako_bubble_coordinator_.LoadConsentUI(profile_);
      metrics_recorder_->LogEditorState(EditorStates::kConsentScreenImpression);
      break;
    case EditorMode::kBlocked:
      mako_bubble_coordinator_.CloseUI();
  }
}

void EditorMediator::CacheContext() {
  mako_bubble_coordinator_.CacheContextCaretBounds();

  size_t selected_length = NonWhitespaceAndSymbolsLength(
      surrounding_text_.text, surrounding_text_.selection_range);
  editor_context_.OnTextSelectionLengthChanged(selected_length);

  if (editor_event_proxy_ != nullptr) {
    editor_event_proxy_->OnSurroundingTextChanged(
        surrounding_text_.text, surrounding_text_.selection_range);
  }
}

void EditorMediator::FetchAndUpdateInputContext() {
  GetTextFieldContextualInfo(
      base::BindOnce(&EditorMediator::OnTextFieldContextualInfoChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EditorMediator::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  editor_context_.OnInputContextUpdated(
      IMEBridge::Get()->GetCurrentInputContext(), info);

  if (system_actuator_ != nullptr) {
    system_actuator_->OnInputContextUpdated(info.tab_url);
  }
}

bool EditorMediator::IsAllowedForUse() {
  return editor_switch_->IsAllowedForUse();
}

EditorMode EditorMediator::GetEditorMode() const {
  if (editor_mode_override_for_testing_.has_value()) {
    return *editor_mode_override_for_testing_;
  }
  return editor_switch_->GetEditorMode();
}

EditorMetricsRecorder* EditorMediator::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

EditorOpportunityMode EditorMediator::GetEditorOpportunityMode() const {
  return editor_switch_->GetEditorOpportunityMode();
}

std::vector<EditorBlockedReason> EditorMediator::GetBlockedReasons() const {
  return editor_switch_->GetBlockedReasons();
}

void EditorMediator::Shutdown() {
  // Note that this method is part of the two-phase shutdown completed by a
  // KeyedService. This method is invoked as the first phase, and is called
  // prior to the destruction of the keyed profile (this allows us to cleanup
  // any resources that depend on a valid profile instance - ie WebUI). The
  // second phase is the destruction of the eKeyedService itself.
  mako_bubble_coordinator_.CloseUI();
  profile_ = nullptr;
  text_query_provider_ = nullptr;
  consent_store_ = nullptr;
  editor_switch_ = nullptr;
}

bool EditorMediator::SetTextQueryProviderResponseForTesting(
    const std::vector<std::string>& mock_results) {
  text_query_provider_->SetProvider(
      std::make_unique<EditorTextQueryFromMemory>(mock_results));  // IN-TEST
  return true;
}

void EditorMediator::OverrideEditorModeForTesting(EditorMode editor_mode) {
  editor_mode_override_for_testing_ = editor_mode;
}

}  // namespace ash::input_method
