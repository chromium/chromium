// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/input_method_test_interface_ash.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace crosapi {
namespace {

ash::InputMethodAsh* GetInputMethod() {
  const ash::IMEBridge* bridge = ash::IMEBridge::Get();
  if (!bridge)
    return nullptr;

  ash::TextInputTarget* handler = bridge->GetInputContextHandler();
  if (!handler)
    return nullptr;

  // Guaranteed to be an ash::InputMethodAsh*.
  return static_cast<ash::InputMethodAsh*>(handler->GetInputMethod());
}

}  // namespace

InputMethodTestInterfaceAsh::InputMethodTestInterfaceAsh()
    : input_method_(GetInputMethod()) {
  DCHECK(input_method_);
  input_method_observation_.Observe(input_method_);
}

InputMethodTestInterfaceAsh::~InputMethodTestInterfaceAsh() = default;

void InputMethodTestInterfaceAsh::WaitForFocus(WaitForFocusCallback callback) {
  // If `GetTextInputClient` is not null, then it's already focused.
  if (input_method_->GetTextInputClient()) {
    std::move(callback).Run();
    return;
  }

  // `callback` is assumed to outlive this class.
  focus_callbacks_.AddUnsafe(std::move(callback));
}

void InputMethodTestInterfaceAsh::CommitText(const std::string& text,
                                             CommitTextCallback callback) {
  input_method_->CommitText(
      base::UTF8ToUTF16(text),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::SetComposition(
    const std::string& text,
    uint32_t index,
    SetCompositionCallback callback) {
  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16(text);

  input_method_->UpdateCompositionText(composition, index, /*visible=*/true);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::SendKeyEvent(mojom::KeyEventPtr event,
                                               SendKeyEventCallback callback) {
  ui::KeyEvent key_press(
      event->type == mojom::KeyEventType::kKeyPress ? ui::ET_KEY_PRESSED
                                                    : ui::ET_KEY_RELEASED,
      static_cast<ui::KeyboardCode>(event->key_code),
      static_cast<ui::DomCode>(event->dom_code), ui::EF_NONE,
      static_cast<ui::DomKey>(event->dom_key), ui::EventTimeForNow());
  input_method_->SendKeyEvent(&key_press);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  // Focus is actually propagated via OnTextInputStateChanged, not
  // OnFocus/OnBlur (which are only used for unit tests).
  if (!client)
    return;

  focus_callbacks_.Notify();
}

}  // namespace crosapi
