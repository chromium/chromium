// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/input_connection_impl.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keymap.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
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
constexpr base::TimeDelta kStateUpdateTimeout = base::Seconds(1);

// Characters which should be sent as a KeyEvent and attributes of generated
// KeyEvent.
constexpr std::tuple<char, ui::KeyboardCode, ui::DomCode, ui::DomKey>
    kControlCharToKeyEvent[] = {
        {'\n', ui::VKEY_RETURN, ui::DomCode::ENTER, ui::DomKey::ENTER}};

bool IsControlChar(const std::u16string& text) {
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
  ash::IMEBridge* bridge = ash::IMEBridge::Get();
  DCHECK(bridge);
  ash::TextInputTarget* handler = bridge->GetInputContextHandler();
  if (!handler)
    return nullptr;
  ui::TextInputClient* client = handler->GetInputMethod()->GetTextInputClient();
  DCHECK(client);
  return client;
}

ui::KeyEvent CreateKeyEvent(ui::EventType type,
                            ui::KeyboardCode key_code,
                            ui::DomCode code,
                            ui::DomKey key) {
  return ui::KeyEvent(type, key_code, code, ui::EF_NONE, key,
                      ui::EventTimeForNow());
}

}  // namespace

InputConnectionImpl::InputConnectionImpl(
    ash::input_method::InputMethodEngine* ime_engine,
    ArcInputMethodManagerBridge* imm_bridge,
    int input_context_id)
    : ime_engine_(ime_engine),
      imm_bridge_(imm_bridge),
      input_context_id_(input_context_id),
      state_update_timer_() {}

InputConnectionImpl::~InputConnectionImpl() = default;

void InputConnectionImpl::Bind(
    mojo::PendingRemote<mojom::InputConnection>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
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
  std::optional<gfx::Range> composition_text_range = gfx::Range();
  std::u16string text;

  if (!client) {
    return mojom::TextInputStatePtr(std::in_place, 0, text, text_range,
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
      std::in_place, selection_range.start(), text, text_range, selection_range,
      client->GetTextInputType(), client->ShouldDoLearning(),
      client->GetTextInputFlags(), is_input_state_update_requested,
      composition_text_range);
}

void InputConnectionImpl::CommitText(const std::u16string& text,
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

  if (!ime_engine_->CommitText(input_context_id_, text, &error))
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

  std::u16string composing_text;
  client->GetTextFromRange(composition_range, &composing_text);

  std::string error;
  if (!ime_engine_->CommitText(input_context_id_, composing_text, &error)) {
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
    const std::u16string& text,
    int new_cursor_pos,
    const std::optional<gfx::Range>& new_selection_range) {
  // It's relative to the last character of the composing text,
  // so 0 means the cursor should be just before the last character of the text.
  new_cursor_pos += text.length() - 1;

  StartStateUpdateTimer();

  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return;

  // Calculate the position of composition insertion point
  gfx::Range selection_range, composition_range;
  client->GetEditableSelectionRange(&selection_range);
  client->GetCompositionTextRange(&composition_range);

  const int insertion_point = composition_range.is_empty()
                                  ? selection_range.start()
                                  : composition_range.start();

  const int selection_start =
      new_selection_range
          ? new_selection_range.value().start() - insertion_point
          : new_cursor_pos;
  const int selection_end =
      new_selection_range ? new_selection_range.value().end() - insertion_point
                          : new_cursor_pos;

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
          std::vector<ash::input_method::InputMethodEngine::SegmentInfo>(),
          &error)) {
    LOG(ERROR) << "SetComposition failed: pos=" << new_cursor_pos
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

void InputConnectionImpl::SendKeyEvent(
    std::unique_ptr<ui::KeyEvent> key_event) {
  DCHECK(key_event);
  std::string error;
  if (!ime_engine_->SendKeyEvents(input_context_id_, {*key_event}, &error)) {
    LOG(ERROR) << error;
  }
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

  ash::input_method::InputMethodEngine::SegmentInfo segment_info;
  segment_info.start = 0;
  segment_info.end = new_composition_range.length();
  segment_info.style =
      ash::input_method::InputMethodEngine::SEGMENT_STYLE_UNDERLINE;

  std::string error;
  if (!ime_engine_->ash::input_method::InputMethodEngine::SetComposingRange(
          input_context_id_, new_composition_range.start(),
          new_composition_range.end(), {segment_info}, &error)) {
    LOG(ERROR) << "SetComposingRange failed: range="
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

void InputConnectionImpl::SendControlKeyEvent(const std::u16string& text) {
  DCHECK(IsControlChar(text));

  const std::string str = base::UTF16ToUTF8(text);
  DCHECK_EQ(1u, str.length());

  for (const auto& t : kControlCharToKeyEvent) {
    if (std::get<0>(t) == str[0]) {
      ui::KeyEvent press =
          CreateKeyEvent(ui::EventType::kKeyPressed, std::get<1>(t),
                         std::get<2>(t), std::get<3>(t));

      ui::KeyEvent release =
          CreateKeyEvent(ui::EventType::kKeyReleased, std::get<1>(t),
                         std::get<2>(t), std::get<3>(t));

      std::string error;
      if (!ime_engine_->SendKeyEvents(input_context_id_, {press, release},
                                      &error)) {
        LOG(ERROR) << error;
      }
      break;
    }
  }
  return;
}

}  // namespace arc
