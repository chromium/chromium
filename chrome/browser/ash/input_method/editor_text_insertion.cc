// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_insertion.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace input_method {
namespace {

constexpr base::TimeDelta kInsertionTimeout = base::Seconds(1);

void ConcatenateParagraphs(std::string_view text, std::string* output) {
  std::string final_string;
  if (base::ReplaceChars(text, "\n", "", &final_string)) {
    *output = final_string;
    return;
  }
  *output = text;
}

}  // namespace

EditorTextInsertion::EditorTextInsertion(
    const std::string& text,
    EditorTextInsertion::InsertionStrategy strategy)
    : pending_text_(text), state_(State::kPending), strategy_(strategy) {
  text_insertion_timeout_.Start(
      FROM_HERE, kInsertionTimeout,
      base::BindOnce(&EditorTextInsertion::CancelTextInsertion,
                     weak_ptr_factory_.GetWeakPtr()));
}

EditorTextInsertion::~EditorTextInsertion() = default;

bool EditorTextInsertion::HasTimedOut() {
  return state_ == State::kTimedOut;
}

bool EditorTextInsertion::Commit() {
  TextInputTarget* input = IMEBridge::Get()->GetInputContextHandler();
  if (HasTimedOut() || !input) {
    return false;
  }
  text_insertion_timeout_.Stop();

  switch (strategy_) {
    case InsertionStrategy::kInsertAsMultipleParagraphs:
      input->CommitText(
          base::UTF8ToUTF16(pending_text_),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      break;

    case InsertionStrategy::kInsertAsASingleParagraph:
      std::string concatenated_text;
      ConcatenateParagraphs(pending_text_, &concatenated_text);
      input->CommitText(
          base::UTF8ToUTF16(concatenated_text),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      break;
  }

  return true;
}

int EditorTextInsertion::GetTextLength() {
  return pending_text_.size();
}

void EditorTextInsertion::CancelTextInsertion() {
  state_ = State::kTimedOut;
}

}  // namespace input_method
}  // namespace ash
