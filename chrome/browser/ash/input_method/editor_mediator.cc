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
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_query_context.h"
#include "chrome/browser/ash/input_method/editor_text_query_from_manta.h"
#include "chrome/browser/ash/input_method/editor_text_query_from_memory.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "chromeos/components/editor_menu/public/cpp/editor_helpers.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash::input_method {
namespace {

constexpr std::u16string_view kAnnouncementViewName = u"Orca";

}

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
                                               metrics_recorder_.get())),
      announcer_(kAnnouncementViewName) {
  editor_context_.OnTabletModeUpdated(
      display::Screen::GetScreen()->InTabletMode());
}

EditorMediator::~EditorMediator() = default;

void EditorMediator::BindEditorClient(
    mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) {
  if (IsServiceConnected()) {
    service_connection_->editor_client_connector()->BindEditorClient(
        std::move(pending_receiver));
  }
}

void EditorMediator::OnEditorServiceConnected(bool is_connection_successful) {}

bool EditorMediator::IsServiceConnected() {
  return editor_service_connector_ && editor_service_connector_->IsBound() &&
         service_connection_;
}

void EditorMediator::ResetEditorConnections() {
  if (editor_service_connector_) {
    service_connection_ = std::make_unique<ServiceConnection>(
        profile_, this, metrics_recorder_.get(),
        editor_service_connector_.get());
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

void EditorMediator::OnImeChange(std::string_view engine_id) {
  if (base::FeatureList::IsEnabled(ash::features::kOrcaServiceConnection) &&
      service_connection_) {
    ResetEditorConnections();
  }
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

  if (IsAllowedForUse() && !editor_service_connector_) {
    editor_service_connector_ =
        std::make_unique<EditorServiceConnector>(&editor_context_);
    ResetEditorConnections();
  }

  if (IsServiceConnected()) {
    service_connection_->system_actuator()->OnFocus(context_id);
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

  if (base::FeatureList::IsEnabled(ash::features::kOrcaServiceConnection) &&
      IsServiceConnected()) {
    ResetEditorConnections();
  }
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
  consent_store_->ProcessPromoCardAction(PromoCardAction::kDecline);
}

void EditorMediator::HandleTrigger(
    std::optional<std::string_view> preset_query_id,
    std::optional<std::string_view> freeform_text) {
  metrics_recorder_->SetTone(preset_query_id, freeform_text);

  EditorQueryContext active_query_context =
      query_context_.has_value()
          ? EditorQueryContext{query_context_->preset_query_id,
                               query_context_->freeform_text}
          : EditorQueryContext{preset_query_id, freeform_text};

  switch (GetEditorMode()) {
    case EditorMode::kRewrite:
      mako_bubble_coordinator_.LoadEditorUI(
          profile_, MakoEditorMode::kRewrite,
          /*can_fallback_to_center_position=*/true,
          /*feedback_enabled=*/editor_switch_->IsFeedbackEnabled(),
          active_query_context.preset_query_id,
          active_query_context.freeform_text);
      query_context_ = std::nullopt;
      metrics_recorder_->LogEditorState(EditorStates::kNativeRequest);
      break;
    case EditorMode::kWrite:
      mako_bubble_coordinator_.LoadEditorUI(
          profile_, MakoEditorMode::kWrite,
          /*can_fallback_to_center_position=*/true,
          /*feedback_enabled=*/editor_switch_->IsFeedbackEnabled(),
          active_query_context.preset_query_id,
          active_query_context.freeform_text);
      query_context_ = std::nullopt;
      metrics_recorder_->LogEditorState(EditorStates::kNativeRequest);
      break;
    case EditorMode::kConsentNeeded:
      query_context_ = EditorQueryContext(/*preset_query_id=*/preset_query_id,
                                          /*freeform_text=*/freeform_text);
      ShowNotice();
      metrics_recorder_->LogEditorState(EditorStates::kConsentScreenImpression);
      break;
    case EditorMode::kHardBlocked:
    case EditorMode::kSoftBlocked:
      mako_bubble_coordinator_.CloseUI();
  }
}

void EditorMediator::ShowNotice() {
  if (chromeos::MagicBoostState::Get()->IsMagicBoostAvailable()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->magic_boost_controller_ash()
        ->ShowDisclaimerUi(
            /*display_id=*/display::Screen::GetScreen()
                ->GetPrimaryDisplay()
                .id(),
            /*action=*/
            crosapi::mojom::MagicBoostController::TransitionAction::
                kShowEditorPanel,
            /*opt_in_features=*/OptInFeatures::kOrcaAndHmr);
  } else {
    mako_bubble_coordinator_.LoadConsentUI(profile_);
  }
}

void EditorMediator::CacheContext() {
  GetTextFieldContextualInfo(
      base::BindOnce(&EditorMediator::OnTextFieldContextualInfoChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  mako_bubble_coordinator_.CacheContextCaretBounds();

  size_t selected_length =
      chromeos::editor_helpers::NonWhitespaceAndSymbolsLength(
          surrounding_text_.text, surrounding_text_.selection_range);
  editor_context_.OnTextSelectionLengthChanged(selected_length);

  if (IsServiceConnected()) {
    service_connection_->editor_event_proxy()->OnSurroundingTextChanged(
        surrounding_text_.text, surrounding_text_.selection_range);
  }
}

void EditorMediator::FetchAndUpdateInputContextForTesting() {
  GetTextFieldContextualInfo(
      base::BindOnce(&EditorMediator::OnTextFieldContextualInfoChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

EditorMediator::ServiceConnection::ServiceConnection(
    Profile* profile,
    EditorMediator* mediator,
    EditorMetricsRecorder* metrics_recorder,
    EditorServiceConnector* service_connector) {
  mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>
      system_actuator_remote;
  mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
      text_query_provider_remote;
  mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
      editor_client_connector_receiver;
  mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
      editor_event_sink_receiver;

  system_actuator_ = std::make_unique<EditorSystemActuator>(
      profile, system_actuator_remote.InitWithNewEndpointAndPassReceiver(),
      mediator);
  text_query_provider_ = std::make_unique<EditorTextQueryProvider>(
      text_query_provider_remote.InitWithNewEndpointAndPassReceiver(),
      metrics_recorder, std::make_unique<EditorTextQueryFromManta>(profile));
  editor_client_connector_ = std::make_unique<EditorClientConnector>(
      editor_client_connector_receiver.InitWithNewEndpointAndPassRemote());
  editor_event_proxy_ = std::make_unique<EditorEventProxy>(
      editor_event_sink_receiver.InitWithNewEndpointAndPassRemote());

  service_connector->BindEditor(std::move(editor_client_connector_receiver),
                                std::move(editor_event_sink_receiver),
                                std::move(system_actuator_remote),
                                std::move(text_query_provider_remote));
}

EditorMediator::ServiceConnection::~ServiceConnection() = default;

EditorEventProxy* EditorMediator::ServiceConnection::editor_event_proxy() {
  return editor_event_proxy_.get();
}

EditorClientConnector*
EditorMediator::ServiceConnection::editor_client_connector() {
  return editor_client_connector_.get();
}

EditorTextQueryProvider*
EditorMediator::ServiceConnection::text_query_provider() {
  return text_query_provider_.get();
}

EditorSystemActuator* EditorMediator::ServiceConnection::system_actuator() {
  return system_actuator_.get();
}

void EditorMediator::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  editor_context_.OnInputContextUpdated(
      IMEBridge::Get()->GetCurrentInputContext(), info);

  if (IsServiceConnected()) {
    service_connection_->system_actuator()->OnInputContextUpdated(info.tab_url);
  }
}

bool EditorMediator::IsAllowedForUse() {
  return editor_switch_->IsAllowedForUse();
}

bool EditorMediator::CanShowNoticeBanner() const {
  return editor_switch_->CanShowNoticeBanner();
}

EditorMode EditorMediator::GetEditorMode() const {
  if (editor_mode_override_for_testing_.has_value()) {
    return *editor_mode_override_for_testing_;
  }
  return editor_switch_->GetEditorMode();
}

ConsentStatus EditorMediator::GetConsentStatus() const {
  return consent_store_->GetConsentStatus();
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
  consent_store_ = nullptr;
  editor_switch_ = nullptr;
}

bool EditorMediator::SetTextQueryProviderResponseForTesting(
    const std::vector<std::string>& mock_results) {
  if (!IsServiceConnected()) {
    return false;
  }
  service_connection_->text_query_provider()->SetProvider(
      std::make_unique<EditorTextQueryFromMemory>(mock_results));  // IN-TEST
  return true;
}

void EditorMediator::OverrideEditorModeForTesting(EditorMode editor_mode) {
  editor_mode_override_for_testing_ = editor_mode;
}

}  // namespace ash::input_method
