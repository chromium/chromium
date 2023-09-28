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
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash::input_method {
namespace {

EditorMediator* g_instance_ = nullptr;

}  // namespace

EditorMediator::EditorMediator(Profile* profile, std::string_view country_code)
    : profile_(profile),
      editor_instance_impl_(this),
      panel_manager_(this),
      editor_switch_(std::make_unique<EditorSwitch>(profile, country_code)),
      consent_store_(
          std::make_unique<EditorConsentStore>(profile->GetPrefs())) {
  DCHECK(!g_instance_);
  g_instance_ = this;

  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  profile_observation_.Observe(profile_);
  tablet_mode_observation_.Observe(TabletMode::Get());

  editor_switch_->OnTabletModeUpdated(ash::TabletMode::IsInTabletMode());
}

EditorMediator::~EditorMediator() {
  DCHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;

  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  }
}

EditorMediator* EditorMediator::Get() {
  return g_instance_;
}

bool EditorMediator::HasInstance() {
  return g_instance_ != nullptr;
}

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
        text_actuator_remote.InitWithNewEndpointAndPassReceiver(), this);
    text_query_provider_ = std::make_unique<EditorTextQueryProvider>(
        text_query_provider_remote.InitWithNewEndpointAndPassReceiver(),
        profile_);
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

  if (editor_event_proxy_ != nullptr) {
    editor_event_proxy_->OnSurroundingTextChanged(text, selection_range);
  }
  editor_switch_->OnTextSelectionLengthChanged(selection_range.length());
}

void EditorMediator::ProcessConsentAction(ConsentAction consent_action) {
  consent_store_->ProcessConsentAction(consent_action);
  HandleTrigger(/*preset_query_id=*/absl::nullopt,
                /*freeform_text=*/absl::nullopt);
}

void EditorMediator::OnPromoCardDeclined() {
  consent_store_->ProcessPromoCardAction(PromoCardAction::kDeclined);
}

void EditorMediator::HandleTrigger(
    absl::optional<std::string_view> preset_query_id,
    absl::optional<std::string_view> freeform_text) {
  switch (GetEditorMode()) {
    case EditorMode::kRewrite:
      mako_bubble_coordinator_.ShowEditorUI(profile_, MakoEditorMode::kRewrite,
                                            preset_query_id, freeform_text);
      break;
    case EditorMode::kWrite:
      mako_bubble_coordinator_.ShowEditorUI(profile_, MakoEditorMode::kWrite,
                                            preset_query_id, freeform_text);
      break;
    case EditorMode::kConsentNeeded:
      mako_bubble_coordinator_.ShowConsentUI(profile_);
      break;
    case EditorMode::kBlocked:
      mako_bubble_coordinator_.CloseUI();
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

void EditorMediator::ActiveUserChanged(user_manager::User* user) {
  if (user) {
    user->AddProfileCreatedObserver(
        base::BindOnce(&EditorMediator::SetProfileByUser,
                       weak_ptr_factory_.GetWeakPtr(), user));
  }
}

void EditorMediator::SetProfileByUser(user_manager::User* user) {
  profile_ = ProfileHelper::Get()->GetProfileByUser(user);
  profile_observation_.Reset();
  profile_observation_.Observe(profile_);
  editor_switch_->SetProfile(profile_);
  consent_store_->SetPrefService(profile_->GetPrefs());
  if (text_query_provider_ != nullptr) {
    text_query_provider_->OnProfileChanged(profile_);
  }
}

void EditorMediator::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();

  mako_bubble_coordinator_.CloseUI();
  profile_ = nullptr;
  consent_store_ = nullptr;
  editor_switch_ = nullptr;
}

}  // namespace ash::input_method
