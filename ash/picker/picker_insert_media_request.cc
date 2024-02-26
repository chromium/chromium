// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media_request.h"

#include "ash/picker/picker_insert_media.h"
#include "ash/picker/picker_rich_media.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

PickerInsertMediaRequest::PickerInsertMediaRequest(
    ui::InputMethod* input_method,
    const PickerRichMedia& media_to_insert,
    const base::TimeDelta insert_timeout,
    InsertFailedCallback insert_failed_callback)
    : media_to_insert(media_to_insert),
      insert_failed_callback_(std::move(insert_failed_callback)) {
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
      !media_to_insert.has_value()) {
    return;
  }

  DCHECK_EQ(mutable_client, client);
  if (!InsertMediaToInputField(*media_to_insert, mutable_client)) {
    if (!insert_failed_callback_.is_null()) {
      std::move(insert_failed_callback_).Run();
    }
  }

  media_to_insert = std::nullopt;
  observation_.Reset();
}

void PickerInsertMediaRequest::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  if (observation_.GetSource() == input_method) {
    observation_.Reset();
  }
}

void PickerInsertMediaRequest::CancelPendingInsert() {
  media_to_insert = std::nullopt;
  observation_.Reset();

  if (!insert_failed_callback_.is_null()) {
    std::move(insert_failed_callback_).Run();
  }
}

}  // namespace ash
