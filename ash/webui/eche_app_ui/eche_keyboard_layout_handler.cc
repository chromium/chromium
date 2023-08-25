// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_keyboard_layout_handler.h"

#include "ash/shell.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::eche_app {

EcheKeyboardLayoutHandler::EcheKeyboardLayoutHandler() {
  Shell::Get()->ime_controller()->AddObserver(this);
}

EcheKeyboardLayoutHandler::~EcheKeyboardLayoutHandler() {
  if (Shell::HasInstance()) {
    Shell::Get()->ime_controller()->RemoveObserver(this);
  }
}

void EcheKeyboardLayoutHandler::RequestCurrentKeyboardLayout() {
  if (!remote_observer_.is_bound()) {
    return;
  }

  const ImeInfo& current_ime = Shell::Get()->ime_controller()->current_ime();
  remote_observer_->OnKeyboardLayoutChanged(
      current_ime.id, base::UTF16ToUTF8(current_ime.name),
      base::UTF16ToUTF8(current_ime.short_name),
      Shell::Get()->ime_controller()->keyboard_layout_name());
}

void EcheKeyboardLayoutHandler::SetKeyboardLayoutObserver(
    mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer) {
  PA_LOG(INFO) << "echeapi EcheKeyboardLayoutHandler SetKeyboardLayoutObserver";
  remote_observer_.reset();
  remote_observer_.Bind(std::move(observer));
}

void EcheKeyboardLayoutHandler::OnCapsLockChanged(bool enabled) {}

void EcheKeyboardLayoutHandler::OnKeyboardLayoutNameChanged(
    const std::string& layout_name) {
  if (!remote_observer_.is_bound()) {
    return;
  }

  const ImeInfo& current_ime = Shell::Get()->ime_controller()->current_ime();
  remote_observer_->OnKeyboardLayoutChanged(
      current_ime.id, base::UTF16ToUTF8(current_ime.name),
      base::UTF16ToUTF8(current_ime.short_name), layout_name);
}

void EcheKeyboardLayoutHandler::Bind(
    mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver) {
  keyboard_layout_handler_.reset();
  keyboard_layout_handler_.Bind(std::move(receiver));
}

}  // namespace ash::eche_app
