// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/input_connection_impl.h"

#include <tuple>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/ime/chromeos/ime_keymap.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

namespace {

// Timeout threshold after the IME operation is sent to TextInputClient.
// If no text input state observer methods in below ArcProxyInputMethodObserver
// is called during this time period, the current text input state is sent to
// Android.
// TODO(yhanada): Implement a way to observe an IME operation completion and
// send the current text input state right after the IME operation completion.
constexpr base::TimeDelta kStateUpdateTimeout = base::TimeDelta::FromSeconds(1);

// Characters which should be sent as a KeyEvent and attributes of generated
// KeyEvent.
constexpr std::tuple<char, ui::KeyboardCode, const char*>
    kControlCharToKeyEvent[] = {{'\n', ui::VKEY_RETURN, "Enter"}};

bool IsControlChar(const base::string16& text) {
  const std::string str = base::UTF16ToUTF8(text);
  if (str.length() != 1)
    return false;
  for (const auto& t : kControlCharToKeyEvent) {
    if (str[0] == std::get<0>(t))
      return true;
  }
  return false;
}

ui::TextInputClient* GetTextInputClient() {
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  DCHECK(bridge);
  ui::IMEInputContextHandlerInterface* handler =
      bridge->GetInputContextHandler();
  if (!handler)
    return nullptr;
  ui::TextInputClient* client = handler->GetInputMethod()->GetTextInputClient();
  DCHECK(client);
  return client;
}

}  // namespace

InputConnectionImpl::InputConnectionImpl(
    chromeos::InputMethodEngine* ime_engine,
    ArcInputMethodManagerBridge* imm_bridge,
    int input_context_id)
    : ime_engine_(ime_engine),
      imm_bridge_(imm_bridge),
      input_context_id_(input_context_id),
      binding_(this),
      state_update_timer_() {}

InputConnectionImpl::~InputConnectionImpl() = default;

void InputConnectionImpl::Bind(mojom::InputConnectionPtr* interface_ptr) {
  binding_.Bind(mojo::MakeRequest(interface_ptr));
}

void InputConnectionImpl::UpdateTextInputState(
    bool is_input_state_update_requested) {
  if (state_update_timer_.IsRunning()) {
    // There is a pending request.
    is_input_state_update_requested = true;
  }
  state_update_timer_.Stop();
  imm_bridge_->SendUpdateTextInputState(
      GetTextInputState(is_input_state_update_requested));
}

mojom::TextInputStatePtr InputConnectionImpl::GetTextInputState(
    bool is_input_state_update_requested) const {
  ui::TextInputClient* client = GetTextInputClient();
  gfx::Range text_range = gfx::Range();
  gfx::Range selection_range = gfx::Range();
  base::Optional<gfx::Range> composition_text_range = gfx::Range();
  base::string16 text;

  if (!client) {
    return mojom::TextInputStatePtr(base::in_place, 0, text, text_range,
                                    selection_range, ui::TEXT_INPUT_TYPE_NONE,
                                    false, 0, is_input_state_update_requested,
                                    composition_text_range);
  }

  client->GetTextRange(&text_range);
  client->GetEditableSelectionRange(&selection_range);
  if (!client->GetCompositionTextRange(&composition_text_range.value()))
    composition_text_range.reset();
  client->GetTextFromRange(text_range, &text);

  return mojom::TextInputStatePtr(
      base::in_place, selection_range.start(), text, text_range,
      selection_range, client->GetTextInputType(), client->ShouldDoLearning(),
      client->GetTextInputFlags(), is_input_state_update_requested,
      composition_text_range);
}

void InputConnectionImpl::CommitText(const base::string16& text,
                                     int new_cursor_pos) {
  StartStateUpdateTimer();

  std::string error;
  // Clear the current composing text at first.
  if (!ime_engine_->ClearComposition(input_context_id_, &error))
    LOG(ERROR) << "ClearComposition failed: error=\"" << error << "\"";

  if (IsControlChar(text)) {
    SendControlKeyEvent(text);
    return;
  }

  if (!ime_engine_->CommitText(input_context_id_,
                               base::UTF16ToUTF8(text).c_str(), &error))
    LOG(ERROR) << "CommitText failed: error=\"" << error << "\"";
}

void InputConnectionImpl::DeleteSurroundingText(int before, int after) {
  StartStateUpdateTimer();

  if (before == 0 && after == 0) {
    // This should be no-op.
    // Return the current state immediately.
    UpdateTextInputState(true);
    return;
  }

  std::string error;
  // DeleteSurroundingText takes a start position relative to the current cursor
  // position and a length of the text is going to be deleted.
  // |before| is a number of characters is going to be deleted before the cursor
  // and |after| is a number of characters is going to be deleted after the
  // cursor.
  if (!ime_engine_->DeleteSurroundingText(input_context_id_, -before,
                                          before + after, &error)) {
    LOG(ERROR) << "DeleteSurroundingText failed: before = " << before
               << ", after = " << after << ", error = \"" << error << "\"";
  }
}

void InputConnectionImpl::FinishComposingText() {
  StartStateUpdateTimer();

  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return;
  gfx::Range selection_range, composition_range;
  client->GetEditableSelectionRange(&selection_range);
  client->GetCompositionTextRange(&composition_range);

  if (composition_range.is_empty()) {
    // There is no ongoing composing. Do nothing.
    UpdateTextInputState(true);
    return;
  }

  base::string16 composing_text;
  client->GetTextFromRange(composition_range, &composing_text);

  std::string error;
  if (!ime_engine_->CommitText(input_context_id_,
                               base::UTF16ToUTF8(composing_text).c_str(),
                               &error)) {
    LOG(ERROR) << "FinishComposingText: CommitText() failed, error=\"" << error
               << "\"";
  }

  if (selection_range.start() == selection_range.end() &&
      selection_range.start() == composition_range.end()) {
    // The call of CommitText won't update the state.
    // Return the current state immediately.
    UpdateTextInputState(true);
  }
}

void InputConnectionImpl::SetComposingText(
    const base::string16& text,
    int new_cursor_pos,
    const base::Optional<gfx::Range>& new_selection_range) {
  // It's relative to the last character of the composing text,
  // so 0 means the cursor should be just before the last character of the text.
  new_cursor_pos += text.length() - 1;

  StartStateUpdateTimer();

  const int selection_start = new_selection_range
                                  ? new_selection_range.value().start()
                                  : new_cursor_pos;
  const int selection_end =
      new_selection_range ? new_selection_range.value().end() : new_cursor_pos;

  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return;

  gfx::Range selection_range;
  client->GetEditableSelectionRange(&selection_range);
  if (text.empty() &&
      selection_range.start() == static_cast<uint32_t>(selection_start) &&
      selection_range.end() == static_cast<uint32_t>(selection_end)) {
    // This SetComposingText call is no-op.
    // Return the current state immediately.
    UpdateTextInputState(true);
  }

  std::string error;
  if (!ime_engine_->SetComposition(
          input_context_id_, base::UTF16ToUTF8(text).c_str(), selection_start,
          selection_end, new_cursor_pos,
          std::vector<input_method::InputMethodEngineBase::SegmentInfo>(),
          &error)) {
    LOG(ERROR) << "SetComposingText failed: pos=" << new_cursor_pos
               << ", error=\"" << error << "\"";
    return;
  }
}

void InputConnectionImpl::RequestTextInputState(
    mojom::InputConnection::RequestTextInputStateCallback callback) {
  std::move(callback).Run(GetTextInputState(false));
}

void InputConnectionImpl::SetSelection(const gfx::Range& new_selection_range) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return;

  gfx::Range selection_range;
  client->GetEditableSelectionRange(&selection_range);
  if (new_selection_range == selection_range) {
    // This SetSelection call is no-op.
    // Return the current state immediately.
    UpdateTextInputState(true);
  }

  StartStateUpdateTimer();
  client->SetEditableSelectionRange(new_selection_range);
}

void InputConnectionImpl::SendKeyEvent(mojom::KeyEventDataPtr data_ptr) {
  chromeos::InputMethodEngine::KeyboardEvent event;
  if (data_ptr->pressed)
    event.type = "keydown";
  else
    event.type = "keyup";

  ui::KeyboardCode key_code = static_cast<ui::KeyboardCode>(data_ptr->key_code);
  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(key_code);

  event.key = ui::KeycodeConverter::DomCodeToCodeString(dom_code);
  event.code = ui::KeyboardCodeToDomKeycode(key_code);
  event.key_code = data_ptr->key_code;
  event.alt_key = data_ptr->is_alt_down;
  event.ctrl_key = data_ptr->is_control_down;
  event.shift_key = data_ptr->is_shift_down;
  event.caps_lock = data_ptr->is_capslock_on;

  ime_engine_->SendKeyEvents(input_context_id_, {event});
}

void InputConnectionImpl::SetCompositionRange(
    const gfx::Range& new_composition_range) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return;

  gfx::Range selection_range = gfx::Range();
  if (!client->GetEditableSelectionRange(&selection_range)) {
    LOG(ERROR)
        << "SetCompositionRange failed: Editable text field is not focused";
    return;
  }

  StartStateUpdateTimer();

  const int before = selection_range.start() - new_composition_range.start();
  const int after = new_composition_range.end() - selection_range.end();
  input_method::InputMethodEngineBase::SegmentInfo segment_info;
  segment_info.start = 0;
  segment_info.end = new_composition_range.length();
  segment_info.style =
      input_method::InputMethodEngineBase::SEGMENT_STYLE_UNDERLINE;

  std::string error;
  if (!ime_engine_->input_method::InputMethodEngineBase::SetCompositionRange(
          input_context_id_, before, after, {segment_info}, &error)) {
    LOG(ERROR) << "SetCompositionRange failed: range="
               << new_composition_range.ToString() << ", error=\"" << error
               << "\"";
  }
}

void InputConnectionImpl::StartStateUpdateTimer() {
  // It's safe to use Unretained() here because the timer is automatically
  // canceled when it go out of scope.
  state_update_timer_.Start(
      FROM_HERE, kStateUpdateTimeout,
      base::BindOnce(&InputConnectionImpl::UpdateTextInputState,
                     base::Unretained(this),
                     true /* is_input_state_update_requested */));
}

void InputConnectionImpl::SendControlKeyEvent(const base::string16& text) {
  DCHECK(IsControlChar(text));

  const std::string str = base::UTF16ToUTF8(text);
  DCHECK_EQ(1u, str.length());

  for (const auto& t : kControlCharToKeyEvent) {
    if (std::get<0>(t) == str[0]) {
      chromeos::InputMethodEngine::KeyboardEvent press;
      press.type = "keydown";
      press.key_code = std::get<1>(t);
      press.key = press.code = std::get<2>(t);
      chromeos::InputMethodEngine::KeyboardEvent release(press);
      release.type = "keyup";
      ime_engine_->SendKeyEvents(input_context_id_, {press, release});
      break;
    }
  }
  return;
}

}  // namespace arc
