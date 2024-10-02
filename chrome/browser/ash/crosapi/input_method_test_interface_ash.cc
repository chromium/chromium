// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/input_method_test_interface_ash.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/crosapi/cpp/input_method_test_interface_constants.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/input_method_manager.h"
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

scoped_refptr<ash::input_method::InputMethodManager::State>
GetInputMethodManagerState() {
  return ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
}

bool HasCapability(std::string_view capability) {
  return false;
}

std::string GenerateUniqueExtensionId() {
  static int counter = 0;

  // Use a static counter to generate unique extension IDs.
  // The extension ID must be 32 characters long, so pad it out.
  std::string extension_id = base::NumberToString(counter++);
  extension_id.append(32 - extension_id.size(), '_');
  return extension_id;
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

void FakeTextInputMethod::SetSurroundingText(const std::u16string& text,
                                             const gfx::Range selection_range,
                                             uint32_t offset_pos) {
  // TODO(b/238838841): Handle `offset_pos`.
  // Don't send surrounding text changed event if the surrounding text hasn't
  // changed.
  if (previous_surrounding_text_ == text &&
      previous_selection_range_ == selection_range) {
    return;
  }

  previous_surrounding_text_ = text;
  previous_selection_range_ = selection_range;

  for (auto& observer : observers_) {
    observer.OnSurroundingTextChanged(text, selection_range);
  }
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
  InstallAndSwitchToInputMethod(mojom::InputMethod::New(/*xkb_layout=*/"us"),
                                base::DoNothing());
  text_input_method_observation_.Observe(&fake_text_input_method_);
}

InputMethodTestInterfaceAsh::~InputMethodTestInterfaceAsh() = default;

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
  ui::KeyEvent key_press(event->type == mojom::KeyEventType::kKeyPress
                             ? ui::EventType::kKeyPressed
                             : ui::EventType::kKeyReleased,
                         static_cast<ui::KeyboardCode>(event->key_code),
                         static_cast<ui::DomCode>(event->dom_code),
                         event->flags, static_cast<ui::DomKey>(event->dom_key),
                         ui::EventTimeForNow());
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

void InputMethodTestInterfaceAsh::WaitForNextSurroundingTextChange(
    WaitForNextSurroundingTextChangeCallback callback) {
  // If there are no queued surrounding text changes, then save the callback to
  // be called by the next surrounding text change. Otherwise, pop the first
  // pending surrounding text and pass it to the callback.
  if (surrounding_text_changes_.empty()) {
    DCHECK(surrounding_text_change_callback_.is_null());
    // `callback` is assumed to outlive this class.
    surrounding_text_change_callback_ = std::move(callback);
    return;
  }
  auto surrounding_text = std::move(surrounding_text_changes_.front());
  surrounding_text_changes_.pop();
  std::move(callback).Run(surrounding_text.text,
                          surrounding_text.selection_range);
}

void InputMethodTestInterfaceAsh::HasCapabilities(
    const std::vector<std::string>& capabilities,
    HasCapabilitiesCallback callback) {
  for (const std::string& capability : capabilities) {
    if (!HasCapability(capability)) {
      std::move(callback).Run(false);
      return;
    }
  }
  std::move(callback).Run(true);
}

void InputMethodTestInterfaceAsh::ConfirmComposition(
    ConfirmCompositionCallback callback) {
  text_input_target_->ConfirmComposition(/*reset_engine=*/false);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::DeleteSurroundingText(
    uint32_t length_before_selection,
    uint32_t length_after_selection,
    DeleteSurroundingTextCallback callback) {
  text_input_target_->DeleteSurroundingText(length_before_selection,
                                            length_after_selection);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::InstallAndSwitchToInputMethod(
    mojom::InputMethodPtr input_method,
    InstallAndSwitchToInputMethodCallback callback) {
  // For testing, only allow one input method to be installed. Replace the
  // previously installed input method with the new one.
  installed_input_method_ = std::make_unique<ScopedInputMethodInstall>(
      *input_method, &fake_text_input_method_);
  GetInputMethodManagerState()->ChangeInputMethod(
      installed_input_method_->GetInputMethodId(), /*show_message=*/false);
  std::move(callback).Run();
}

void InputMethodTestInterfaceAsh::OnFocus() {
  focus_callbacks_.Notify();
}

void InputMethodTestInterfaceAsh::OnSurroundingTextChanged(
    const std::u16string& text,
    const gfx::Range& selection_range) {
  std::vector<size_t> offsets = {selection_range.start(),
                                 selection_range.end()};
  const std::string text_utf8 =
      base::UTF16ToUTF8AndAdjustOffsets(text, &offsets);
  const gfx::Range selection_range_utf8(offsets[0], offsets[1]);

  // If there is no pending WaitForNextSurroundingTextChange callback, queue the
  // surrounding text change to be returned by the next
  // WaitForNextSurroundingTextChange call. Otherwise, resolve the pending
  // callback with the current surrounding text change.
  if (surrounding_text_change_callback_.is_null()) {
    surrounding_text_changes_.push({text_utf8, selection_range_utf8});
    return;
  }

  std::move(surrounding_text_change_callback_)
      .Run(text_utf8, selection_range_utf8);
}

InputMethodTestInterfaceAsh::ScopedInputMethodInstall::ScopedInputMethodInstall(
    const mojom::InputMethod& input_method,
    ash::TextInputMethod* text_input_method)
    : extension_id_(GenerateUniqueExtensionId()) {
  const std::string input_method_id = GetInputMethodId();

  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      GetInputMethodManagerState();
  ime_state->SetEnabledExtensionImes(std::vector<std::string>{input_method_id});
  ime_state->AddInputMethodExtension(
      extension_id_,
      {ash::input_method::InputMethodDescriptor(
          input_method_id, "", /*indicator=*/"T", input_method.xkb_layout, {},
          /*is_login_keyboard=*/true, {}, {},
          /*handwriting_language=*/std::nullopt)},
      text_input_method);
}

InputMethodTestInterfaceAsh::ScopedInputMethodInstall::
    ~ScopedInputMethodInstall() {
  GetInputMethodManagerState()->RemoveInputMethodExtension(extension_id());
}

const std::string&
InputMethodTestInterfaceAsh::ScopedInputMethodInstall::extension_id() const {
  return extension_id_;
}

std::string
InputMethodTestInterfaceAsh::ScopedInputMethodInstall::GetInputMethodId()
    const {
  return ash::extension_ime_util::GetInputMethodID(extension_id_,
                                                   /*engine_id=*/"test");
}

}  // namespace crosapi
