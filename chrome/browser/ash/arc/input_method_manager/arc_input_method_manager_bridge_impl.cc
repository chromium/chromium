// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"

#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"

namespace arc {

ArcInputMethodManagerBridgeImpl::ArcInputMethodManagerBridgeImpl(
    Delegate* delegate,
    ArcBridgeService* bridge_service)
    : delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->input_method_manager()->SetHost(this);
  bridge_service_->input_method_manager()->AddObserver(this);
}

ArcInputMethodManagerBridgeImpl::~ArcInputMethodManagerBridgeImpl() {
  // It's okay not to call OnConnectionClosed() at all once shutdown starts.
  bridge_service_->input_method_manager()->RemoveObserver(this);
  bridge_service_->input_method_manager()->SetHost(nullptr);
}

void ArcInputMethodManagerBridgeImpl::SendEnableIme(
    const std::string& ime_id,
    bool enable,
    EnableImeCallback callback) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), EnableIme);
  if (!imm_instance)
    return;

  imm_instance->EnableIme(ime_id, enable, std::move(callback));
}

void ArcInputMethodManagerBridgeImpl::SendSwitchImeTo(
    const std::string& ime_id,
    SwitchImeToCallback callback) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), SwitchImeTo);
  if (!imm_instance)
    return;

  imm_instance->SwitchImeTo(ime_id, std::move(callback));
}

void ArcInputMethodManagerBridgeImpl::SendFocus(
    mojo::PendingRemote<mojom::InputConnection> connection,
    mojom::TextInputStatePtr state) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), Focus);
  if (!imm_instance)
    return;

  imm_instance->Focus(std::move(connection), std::move(state));
}

void ArcInputMethodManagerBridgeImpl::SendUpdateTextInputState(
    mojom::TextInputStatePtr state) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), UpdateTextInputState);
  if (!imm_instance)
    return;

  imm_instance->UpdateTextInputState(std::move(state));
}

void ArcInputMethodManagerBridgeImpl::SendShowVirtualKeyboard() {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), ShowVirtualKeyboard);
  if (!imm_instance)
    return;

  imm_instance->ShowVirtualKeyboard();
}

void ArcInputMethodManagerBridgeImpl::SendHideVirtualKeyboard() {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), HideVirtualKeyboard);
  if (!imm_instance)
    return;

  imm_instance->HideVirtualKeyboard();
}

void ArcInputMethodManagerBridgeImpl::OnConnectionClosed() {
  delegate_->OnConnectionClosed();
}

void ArcInputMethodManagerBridgeImpl::OnActiveImeChanged(
    const std::string& ime_id) {
  delegate_->OnActiveImeChanged(ime_id);
}

void ArcInputMethodManagerBridgeImpl::OnImeDisabled(const std::string& ime_id) {
  delegate_->OnImeDisabled(ime_id);
}

void ArcInputMethodManagerBridgeImpl::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  delegate_->OnImeInfoChanged(std::move(ime_info_array));
}

}  // namespace arc
