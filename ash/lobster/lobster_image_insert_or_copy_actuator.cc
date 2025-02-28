// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_insert_or_copy_actuator.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "build/branding_buildflags.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kLobsterToastId[] = "lobster_toast";

}  // namespace

void CopyToClipboard(const std::string& image_bytes) {
  GURL image_data_url(base::StrCat(
      {"data:image/jpeg;base64,", base::Base64Encode(image_bytes)}));

  // Overwrite the clipboard data with the image data url.
  auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste);

  clipboard->WriteHTML(base::UTF8ToUTF16(base::StrCat(
                           {"<img src=\"", image_data_url.spec(), "\">"})),
                       /*source_url=*/"");
}

bool InsertImageOrCopyToClipboard(ui::TextInputClient* input_client,
                                  const std::string& image_bytes) {
  GURL image_data_url(base::StrCat(
      {"data:image/jpeg;base64,", base::Base64Encode(image_bytes)}));

  if (!image_data_url.is_valid()) {
    LOG(ERROR) << "The image data is broken.";
    return false;
  }

  if (!input_client) {
    LOG(ERROR) << "No valid input client found.";
    return false;
  }

  if (input_client->CanInsertImage()) {
    input_client->InsertImage(image_data_url);
    return true;
  }

  CopyToClipboard(image_bytes);

  // Display a toast message.
  ToastManager::Get()->Show(
      ToastData(kLobsterToastId, ToastCatalogName::kCopyImageToClipboardAction,
                l10n_util::GetStringUTF16(
                    IDS_LOBSTER_COPY_IMAGE_TO_CLIPBOARD_TOAST_MESSAGE)));

  return false;
}

}  // namespace ash
