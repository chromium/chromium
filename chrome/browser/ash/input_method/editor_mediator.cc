// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include "ash/constants/ash_pref_names.h"
#include "base/check_op.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "ui/base/ime/ash/ime_bridge.h"

namespace ash {
namespace input_method {
namespace {

EditorMediator* g_instance_ = nullptr;

}  // namespace

EditorMediator::EditorMediator(Profile* profile, std::string_view country_code)
    : profile_(profile),
      editor_instance_impl_(this),
      editor_switch_(std::make_unique<EditorSwitch>(profile, country_code)),
      consent_store_(
          std::make_unique<EditorConsentStore>(profile->GetPrefs())) {
  DCHECK(!g_instance_);
  g_instance_ = this;

  profile_observation_.Observe(profile_);
}

EditorMediator::~EditorMediator() {
  DCHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;
}

EditorMediator* EditorMediator::Get() {
  return g_instance_;
}

bool EditorMediator::HasInstance() {
  return g_instance_ != nullptr;
}

void EditorMediator::BindEditorInstance(
    mojo::PendingReceiver<mojom::EditorInstance> pending_receiver) {
  editor_instance_impl_.BindReceiver(std::move(pending_receiver));
}

void EditorMediator::HandleTrigger() {
  mako_page_handler_ = std::make_unique<ash::MakoPageHandler>();
}

void EditorMediator::OnFocus(int context_id) {
  GetTextFieldContextualInfo(
      base::BindOnce(&EditorMediator::OnTextFieldContextualInfoChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  text_actuator_.OnFocus(context_id);
}

void EditorMediator::OnBlur() {
  text_actuator_.OnBlur();
}

void EditorMediator::OnActivateIme(std::string_view engine_id) {
  editor_switch_->OnActivateIme(engine_id);
}

void EditorMediator::OnConsentActionReceived(ConsentAction consent_action) {
  consent_store_->ProcessConsentAction(consent_action);
}

void EditorMediator::CommitEditorResult(std::string_view text) {
  // This assumes that focus will return to the original text input client after
  // the mako web ui is hidden from view. Thus we queue the text to be inserted
  // here rather then insert it directly into the input.
  text_actuator_.InsertTextOnNextFocus(text);
  // After queuing the text to be inserted, closing the mako web ui should
  // return the focus back to the original input.
  if (mako_page_handler_ != nullptr) {
    mako_page_handler_->CloseUI();
    mako_page_handler_ = nullptr;
  }
}

void EditorMediator::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  editor_switch_->OnInputContextUpdated(
      IMEBridge::Get()->GetCurrentInputContext(), info);
}

bool EditorMediator::IsAllowedForUse() {
  return editor_switch_->IsAllowedForUse();
}

bool EditorMediator::CanBeTriggered() {
  return editor_switch_->CanBeTriggered();
}

ConsentStatus EditorMediator::GetConsentStatus() {
  return consent_store_->GetConsentStatus();
}

void EditorMediator::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();

  profile_ = nullptr;
  consent_store_ = nullptr;
  editor_switch_ = nullptr;
}

}  // namespace input_method
}  // namespace ash
