// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media_request.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

PickerInsertMediaRequest::MediaData PickerInsertMediaRequest::MediaData::Text(
    std::u16string_view text) {
  return MediaData(MediaData::Type::kText, std::u16string(text));
}

PickerInsertMediaRequest::MediaData PickerInsertMediaRequest::MediaData::Text(
    std::string_view text) {
  return MediaData(MediaData::Type::kText, base::UTF8ToUTF16(text));
}

PickerInsertMediaRequest::MediaData PickerInsertMediaRequest::MediaData::Image(
    const GURL& url) {
  return MediaData(MediaData::Type::kImage, url);
}

PickerInsertMediaRequest::MediaData PickerInsertMediaRequest::MediaData::Link(
    const GURL& url) {
  return MediaData(MediaData::Type::kLink, url);
}

PickerInsertMediaRequest::MediaData::MediaData(const MediaData&) = default;

PickerInsertMediaRequest::MediaData&
PickerInsertMediaRequest::MediaData::operator=(const MediaData&) = default;

PickerInsertMediaRequest::MediaData::~MediaData() = default;

bool PickerInsertMediaRequest::MediaData::Insert(ui::TextInputClient* client) {
  switch (type_) {
    case MediaData::Type::kText:
      client->InsertText(
          std::get<std::u16string>(data_),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      return true;
    case MediaData::Type::kImage:
      if (!client->CanInsertImage()) {
        return false;
      }
      client->InsertImage(std::get<GURL>(data_));
      return true;
    case MediaData::Type::kLink:
      // TODO(b/322729192): Insert a real hyperlink.
      client->InsertText(
          base::UTF8ToUTF16(std::get<GURL>(data_).spec()),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      return true;
  }
}

PickerInsertMediaRequest::MediaData::MediaData(Type type, Data data)
    : type_(type), data_(std::move(data)) {}

PickerInsertMediaRequest::PickerInsertMediaRequest(
    ui::InputMethod* input_method,
    const MediaData& data_to_insert,
    const base::TimeDelta insert_timeout,
    InsertFailedCallback insert_failed_callback)
    : data_to_insert(data_to_insert),
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
      !data_to_insert.has_value()) {
    return;
  }

  DCHECK_EQ(mutable_client, client);
  if (!data_to_insert->Insert(mutable_client)) {
    if (!insert_failed_callback_.is_null()) {
      std::move(insert_failed_callback_).Run();
    }
  }

  data_to_insert = std::nullopt;
  observation_.Reset();
}

void PickerInsertMediaRequest::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  if (observation_.GetSource() == input_method) {
    observation_.Reset();
  }
}

void PickerInsertMediaRequest::CancelPendingInsert() {
  data_to_insert = std::nullopt;
  observation_.Reset();

  if (!insert_failed_callback_.is_null()) {
    std::move(insert_failed_callback_).Run();
  }
}

}  // namespace ash
