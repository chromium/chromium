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

ash::InputMethodAsh* GetTextInputTarget() {
  const ash::IMEBridge* bridge = ash::IMEBridge::Get();
  if (!bridge)
    return nullptr;

  ash::TextInputTarget* handler = bridge->GetInputContextHandler();
  if (!handler)
    return nullptr;

  // Guaranteed to be an ash::InputMethodAsh*.
  return static_cast<ash::InputMethodAsh*>(handler->GetInputMethod());
}

void OverrideTextInputMethod(ash::TextInputMethod* text_input_method) {
  ash::IMEBridge* bridge = ash::IMEBridge::Get();
  if (!bridge) {
    return;
  }

  bridge->SetCurrentEngineHandler(text_input_method);
}

}  // namespace

FakeTextInputMethod::FakeTextInputMethod() = default;

FakeTextInputMethod::~FakeTextInputMethod() = default;

void FakeTextInputMethod::Focus(const InputContext& input_context) {
  for (auto& observer : observers_) {
    observer.OnFocus();
  }
}

ui::VirtualKeyboardController*
FakeTextInputMethod::GetVirtualKeyboardController() const {
  return nullptr;
}

bool FakeTextInputMethod::IsReadyForTesting() {
  return true;
}

void FakeTextInputMethod::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                          KeyEventDoneCallback callback) {
  ++current_key_event_id_;
  pending_key_event_callbacks_.emplace(current_key_event_id_,
                                       std::move(callback));
}

void FakeTextInputMethod::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeTextInputMethod::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

uint64_t FakeTextInputMethod::GetCurrentKeyEventId() const {
  return current_key_event_id_;
}

void FakeTextInputMethod::KeyEventHandled(uint64_t key_event_id, bool handled) {
  if (const auto it = pending_key_event_callbacks_.find(key_event_id);
      it != pending_key_event_callbacks_.end()) {
    std::move(it->second)
        .Run(handled ? ui::ime::KeyEventHandledState::kHandledByIME
                     : ui::ime::KeyEventHandledState::kNotHandled);
    pending_key_event_callbacks_.erase(it);
  }
}

InputMethodTestInterfaceAsh::InputMethodTestInterfaceAsh()
    : text_input_target_(GetTextInputTarget()) {
  DCHECK(text_input_target_);
  OverrideTextInputMethod(&fake_text_input_method_);
  text_input_method_observation_.Observe(&fake_text_input_method_);
}

InputMethodTestInterfaceAsh::~InputMethodTestInterfaceAsh() {
  OverrideTextInputMethod(nullptr);
}

void InputMethodTestInterfaceAsh::WaitForFocus(WaitForFocusCallback callback) {
  // If `GetTextInputClient` is not null, then it's already focused.
  if (text_input_target_->GetTextInputClient()) {
    std::move(callback).Run();
    return;
  }

  // `callback` is assumed to outlive this class.
  focus_callbacks_.AddUnsafe(std::move(callback));
}

void InputMethodTestInterfaceAsh::CommitText(const std::string& text,
                                             CommitTextCallback callback) {
  text_input_target_->CommitText(
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

  text_input_target_->UpdateCompositionText(composition, index,
                                            /*visible=*/true);
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
  text_input_target_->SendKeyEvent(&key_press);
  std::move(callback).Run(fake_text_input_method_.GetCurrentKeyEventId());
}

void InputMethodTestInterfaceAsh::KeyEventHandled(
    uint64_t key_event_id,
    bool handled,
    KeyEventHandledCallback callback) {
  fake_text_input_method_.KeyEventHandled(key_event_id, handled);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::OnFocus() {
  focus_callbacks_.Notify();
}

}  // namespace crosapi
