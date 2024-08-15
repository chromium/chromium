// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_actuator.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

LobsterImageActuator::LobsterImageActuator() {}

LobsterImageActuator::~LobsterImageActuator() = default;

void LobsterImageActuator::InsertImageOrCopyToClipboard(
    ui::TextInputClient* input_client,
    const std::string& image_bytes) {
  if (!input_client) {
    LOG(ERROR) << "No valid input client found.";
  }

  GURL image_data_url(base::StrCat(
      {"data:image/jpeg;base64,", base::Base64Encode(image_bytes)}));

  if (!image_data_url.is_valid()) {
    return;
  }

  if (input_client->CanInsertImage()) {
    input_client->InsertImage(image_data_url);
  } else {
    // Overwrite the clipboard data with the image data url.
    auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste);

    clipboard->WriteHTML(base::UTF8ToUTF16(base::StrCat(
                             {"<img src=\"", image_data_url.spec(), "\">"})),
                         /*source_url=*/"");

    // TODO:b: - Show a toast notification if needed.
  }
}

}  // namespace ash
