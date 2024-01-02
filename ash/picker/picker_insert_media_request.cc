// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media_request.h"

#include "base/time/time.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

PickerInsertMediaRequest::PickerInsertMediaRequest(
    ui::InputMethod* input_method,
    const std::u16string_view text_to_insert,
    const base::TimeDelta insert_timeout)
    : text_to_insert_(text_to_insert) {
  observation_.Observe(input_method);
  insert_timeout_timer_.Start(FROM_HERE, insert_timeout, this,
                              &PickerInsertMediaRequest::CancelPendingInsert);
}

PickerInsertMediaRequest::~PickerInsertMediaRequest() = default;

void PickerInsertMediaRequest::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  ui::TextInputClient* mutable_client =
      observation_.GetSource()->GetTextInputClient();
  if (mutable_client == nullptr ||
      mutable_client->GetTextInputType() ==
          ui::TextInputType::TEXT_INPUT_TYPE_NONE ||
      !text_to_insert_.has_value()) {
    return;
  }

  DCHECK_EQ(mutable_client, client);

  mutable_client->InsertText(
      *text_to_insert_,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  text_to_insert_ = std::nullopt;
  observation_.Reset();
}

void PickerInsertMediaRequest::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  if (observation_.GetSource() == input_method) {
    observation_.Reset();
  }
}

void PickerInsertMediaRequest::CancelPendingInsert() {
  text_to_insert_ = std::nullopt;
  observation_.Reset();
}

}  // namespace ash
