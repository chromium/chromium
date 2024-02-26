// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include "ash/picker/picker_rich_media.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

bool InsertMediaToInputField(PickerRichMedia media,
                             ui::TextInputClient* client) {
  if (client == nullptr) {
    return false;
  }

  return std::visit(
      base::Overloaded{
          [client](PickerTextMedia media) {
            client->InsertText(media.text,
                               ui::TextInputClient::InsertTextCursorBehavior::
                                   kMoveCursorAfterText);
            return true;
          },
          [client](PickerImageMedia media) {
            if (!client->CanInsertImage()) {
              return false;
            }
            client->InsertImage(media.url);
            return true;
          },
          [client](PickerLinkMedia media) {
            // TODO(b/322729192): Insert a real hyperlink.
            client->InsertText(base::UTF8ToUTF16(media.url.spec()),
                               ui::TextInputClient::InsertTextCursorBehavior::
                                   kMoveCursorAfterText);
            return true;
          },
      },
      std::move(media));
}

}  // namespace ash
