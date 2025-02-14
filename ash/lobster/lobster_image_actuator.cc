// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_actuator.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

constexpr int kQueryCharLimit = 230;
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
  ToastManager::Get()->Show(ToastData(
      kLobsterToastId, ToastCatalogName::kCopyImageToClipboardAction,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      l10n_util::GetStringUTF16(
          IDS_ASH_LOBSTER_COPY_IMAGE_TO_CLIPBOARD_TOAST_MESSAGE)
#else
      u""
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
          ));

  return false;
}

void WriteImageToPath(const base::FilePath& save_dir,
                      const std::string& query,
                      uint32_t id,
                      const std::string& image_bytes,
                      LobsterImageDownloadResponseCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const base::FilePath& save_dir, const std::string& query,
             uint32_t id,
             std::string_view image_bytes) -> LobsterImageDownloadResponse {
            std::string sanitized_file_name = query;

            // Makes the file name valid.
            base::i18n::ReplaceIllegalCharactersInPath(&sanitized_file_name,
                                                       '-');
            std::string trimmed_file_name =
                sanitized_file_name.substr(0, kQueryCharLimit);

            base::FilePath saved_file_path =
                base::FeatureList::IsEnabled(
                    features::kLobsterFileNamingImprovement)
                    ? base::GetUniquePathWithSuffixFormat(
                          save_dir.Append(
                              base::StringPrintf("%s.jpeg", trimmed_file_name)),
                          "-%d")
                    : save_dir.Append(base::StringPrintf(
                          "%s-%d.jpeg", trimmed_file_name, id));

            if (saved_file_path == base::FilePath() ||
                base::PathExists(saved_file_path)) {
              LOG(ERROR) << "File name already exists: " << saved_file_path;
              return {.download_path = saved_file_path, .success = false};
            }

            if (base::WriteFile(saved_file_path, image_bytes)) {
              return {.download_path = saved_file_path, .success = true};
            }

            LOG(ERROR) << "Unable to write file name " << saved_file_path;
            return {.download_path = saved_file_path, .success = false};
          },
          save_dir, query, id, image_bytes),
      std::move(callback));
}

}  // namespace ash
