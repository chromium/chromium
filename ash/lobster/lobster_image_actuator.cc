// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_actuator.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

void InsertImageOrCopyToClipboard(ui::TextInputClient* input_client,
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

void WriteImageToPath(const base::FilePath& file_path,
                      const std::string& image_bytes) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const base::FilePath& file_path, std::string_view image_bytes) {
            return base::WriteFile(file_path, image_bytes);
          },
          file_path, image_bytes),
      base::BindOnce(
          [](const base::FilePath& file_path, bool success) {
            if (!success) {
              LOG(ERROR) << "Fail to write image to path: " << file_path;
            }
          },
          file_path));
}

}  // namespace ash
