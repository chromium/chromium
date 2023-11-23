// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider_for_testing.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash::input_method {

EditorMediator::EditorMediator(Profile* profile, std::string_view country_code)
    : profile_(profile),
      panel_manager_(this),
      editor_switch_(std::make_unique<EditorSwitch>(profile, country_code)),
      consent_store_(
          std::make_unique<EditorConsentStore>(profile->GetPrefs(),
                                               editor_switch_.get())) {
  tablet_mode_observation_.Observe(TabletMode::Get());
  editor_switch_->OnTabletModeUpdated(ash::TabletMode::IsInTabletMode());
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
    mojo::PendingAssociatedRemote<orca::mojom::TextActuator>
        text_actuator_remote;
    mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
        text_query_provider_remote;
    mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
        editor_client_connector_receiver;
    mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
        editor_event_sink_receiver;

    text_actuator_ = std::make_unique<EditorTextActuator>(
        profile_, text_actuator_remote.InitWithNewEndpointAndPassReceiver(),
        this);
    text_query_provider_ = std::make_unique<TextQueryProviderForOrca>(
        text_query_provider_remote.InitWithNewEndpointAndPassReceiver(),
        profile_, editor_switch_.get());
    editor_client_connector_ = std::make_unique<EditorClientConnector>(
        editor_client_connector_receiver.InitWithNewEndpointAndPassRemote());
    editor_event_proxy_ = std::make_unique<EditorEventProxy>(
        editor_event_sink_receiver.InitWithNewEndpointAndPassRemote());

    editor_service_connector_.BindEditor(
        std::move(editor_client_connector_receiver),
        std::move(editor_event_sink_receiver), std::move(text_actuator_remote),
        std::move(text_query_provider_remote));

    // TODO: b:300838514 - We should only bind the native UI with the shared lib when the
    // Rewrite UI is shown. Consider add a listener to the write/rewrite UI and
    // move the binding there.
    panel_manager_.BindEditorClient();
  }
}

void EditorMediator::BindEditorPanelManager(
    mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
        pending_receiver) {
  panel_manager_.BindReceiver(std::move(pending_receiver));
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

  if (text_actuator_ != nullptr) {
    text_actuator_->OnFocus(context_id);
  }
}

void EditorMediator::OnBlur() {
  if (mako_bubble_coordinator_.IsShowingUI() ||
      panel_manager_.IsEditorMenuVisible()) {
    return;
  }

  if (text_actuator_ != nullptr) {
    text_actuator_->OnBlur();
  }
}

void EditorMediator::OnActivateIme(std::string_view engine_id) {
  editor_switch_->OnActivateIme(engine_id);
}

void EditorMediator::OnTabletModeStarting() {
  editor_switch_->OnTabletModeUpdated(/*tablet_mode_enabled=*/true);
  if (mako_bubble_coordinator_.IsShowingUI()) {
    mako_bubble_coordinator_.CloseUI();
  }
}

void EditorMediator::OnTabletModeEnded() {
  editor_switch_->OnTabletModeUpdated(/*tablet_mode_enabled=*/false);
}

void EditorMediator::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void EditorMediator::OnSurroundingTextChanged(const std::u16string& text,
                                              gfx::Range selection_range) {
  if (mako_bubble_coordinator_.IsShowingUI() ||
      panel_manager_.IsEditorMenuVisible()) {
    return;
  }

  surrounding_text_ = {.text = text, .selection_range = selection_range};

  size_t selected_length = NonWhitespaceAndSymbolsLength(text, selection_range);
  editor_switch_->OnTextSelectionLengthChanged(selected_length);
}

void EditorMediator::ProcessConsentAction(ConsentAction consent_action) {
  consent_store_->ProcessConsentAction(consent_action);
  HandleTrigger(/*preset_query_id=*/absl::nullopt,
                /*freeform_text=*/absl::nullopt);
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

void EditorMediator::OnPromoCardDeclined() {
  consent_store_->ProcessPromoCardAction(PromoCardAction::kDeclined);
}

void EditorMediator::HandleTrigger(
    absl::optional<std::string_view> preset_query_id,
    absl::optional<std::string_view> freeform_text) {
  switch (GetEditorMode()) {
    case EditorMode::kRewrite:
      mako_bubble_coordinator_.LoadEditorUI(profile_, MakoEditorMode::kRewrite,
                                            preset_query_id, freeform_text);
      LogEditorState(EditorStates::kNativeRequest, EditorMode::kRewrite);
      break;
    case EditorMode::kWrite:
      mako_bubble_coordinator_.LoadEditorUI(profile_, MakoEditorMode::kWrite,
                                            preset_query_id, freeform_text);
      LogEditorState(EditorStates::kNativeRequest, EditorMode::kWrite);
      break;
    case EditorMode::kConsentNeeded:
      mako_bubble_coordinator_.LoadConsentUI(profile_);
      break;
    case EditorMode::kBlocked:
      mako_bubble_coordinator_.CloseUI();
  }
}

void EditorMediator::CacheContext() {
  mako_bubble_coordinator_.CacheContextCaretBounds();
  if (editor_event_proxy_ != nullptr) {
    editor_event_proxy_->OnSurroundingTextChanged(
        surrounding_text_.text, surrounding_text_.selection_range);
  }
}

void EditorMediator::OnTextInserted() {
  // After queuing the text to be inserted, closing the mako web ui should
  // return the focus back to the original input.
  mako_bubble_coordinator_.CloseUI();
}

void EditorMediator::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  editor_switch_->OnInputContextUpdated(
      IMEBridge::Get()->GetCurrentInputContext(), info);
}

bool EditorMediator::IsAllowedForUse() {
  return editor_switch_->IsAllowedForUse();
}

EditorMode EditorMediator::GetEditorMode() const {
  return editor_switch_->GetEditorMode();
}

EditorOpportunityMode EditorMediator::GetEditorOpportunityMode() const {
  return editor_switch_->GetEditorOpportunityMode();
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
  auto pending_receiver = text_query_provider_->Unbind();

  if (!pending_receiver.has_value()) {
    return false;
  }
  text_query_provider_ = std::make_unique<TextQueryProviderForTesting>(
      std::move(pending_receiver.value()), mock_results);  // IN-TEST
  return true;
}

}  // namespace ash::input_method
