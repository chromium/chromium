// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include <utility>

#include "ash/picker/picker_rich_media.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

bool InputFieldSupportsInsertingMedia(const PickerRichMedia& media,
                                      ui::TextInputClient& client) {
  return std::visit(base::Overloaded{
                        [](const PickerTextMedia& media) { return true; },
                        [&client](const PickerImageMedia& media) {
                          return client.CanInsertImage();
                        },
                        [](const PickerLinkMedia& media) { return true; },
                    },
                    media);
}

void InsertMediaToInputField(PickerRichMedia media,
                             ui::TextInputClient& client,
                             OnInsertMediaCompleteCallback callback) {
  std::visit(
      base::Overloaded{
          [&client, &callback](PickerTextMedia media) mutable {
            client.InsertText(media.text,
                              ui::TextInputClient::InsertTextCursorBehavior::
                                  kMoveCursorAfterText);
            std::move(callback).Run(InsertMediaResult::kSuccess);
          },
          [&client, &callback](PickerImageMedia media) mutable {
            if (!client.CanInsertImage()) {
              std::move(callback).Run(InsertMediaResult::kUnsupported);
              return;
            }
            client.InsertImage(media.url);
            std::move(callback).Run(InsertMediaResult::kSuccess);
          },
          [&client, &callback](PickerLinkMedia media) mutable {
            // TODO(b/322729192): Insert a real hyperlink.
            client.InsertText(base::UTF8ToUTF16(media.url.spec()),
                              ui::TextInputClient::InsertTextCursorBehavior::
                                  kMoveCursorAfterText);
            std::move(callback).Run(InsertMediaResult::kSuccess);
          },
      },
      std::move(media));
}

}  // namespace ash
