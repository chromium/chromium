// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_actuator.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace input_method {
namespace {

void InsertText(std::string_view text) {
  TextInputTarget* input = IMEBridge::Get()->GetInputContextHandler();
  if (!input) {
    return;
  }
  input->CommitText(
      base::UTF8ToUTF16(text),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

}  // namespace

EditorTextActuator::EditorTextActuator() = default;
EditorTextActuator::~EditorTextActuator() = default;

void EditorTextActuator::InsertTextOnNextFocus(std::string_view text) {
  pending_text_insert_ = PendingTextInsert{std::string(text)};
}

void EditorTextActuator::OnFocus(int context_id) {
  if (focused_client_.has_value() && focused_client_->id == context_id) {
    return;
  }
  focused_client_ = TextClientContext{context_id};
  if (pending_text_insert_) {
    InsertText(pending_text_insert_->text);
    pending_text_insert_ = absl::nullopt;
  }
}

void EditorTextActuator::OnBlur() {
  focused_client_ = absl::nullopt;
}

}  // namespace input_method
}  // namespace ash
