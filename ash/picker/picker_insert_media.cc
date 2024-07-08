// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include <utility>

#include "ash/picker/picker_rich_media.h"
#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "net/base/mime_util.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {
namespace {

std::optional<std::string> GetMediaTypeFromFilePath(
    const base::FilePath& path) {
  std::string mime_type;
  if (!net::GetMimeTypeFromFile(path, &mime_type)) {
    return std::nullopt;
  }
  return mime_type;
}

std::optional<std::string> ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    LOG(WARNING) << "Failed reading file";
    return std::nullopt;
  }
  return result;
}

void ReadFileAsync(
    base::FilePath path,
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileToString, std::move(path)), std::move(callback));
}

std::optional<GURL> ConvertToDataUrl(std::string_view media_type,
                                     std::optional<std::string> data) {
  if (!data.has_value()) {
    return std::nullopt;
  }
  return GURL(base::StrCat(
      {"data:", media_type, ";base64,", base::Base64Encode(*data)}));
}

}  // namespace

bool InputFieldSupportsInsertingMedia(const PickerRichMedia& media,
                                      ui::TextInputClient& client) {
  return std::visit(base::Overloaded{
                        [](const PickerTextMedia& media) { return true; },
                        [](const PickerLinkMedia& media) { return true; },
                        [&client](const PickerLocalFileMedia& media) {
                          return client.CanInsertImage();
                        },
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
          [&client, &callback](PickerLinkMedia media) mutable {
            // TODO(b/322729192): Insert a real hyperlink.
            client.InsertText(base::UTF8ToUTF16(media.url.spec()),
                              ui::TextInputClient::InsertTextCursorBehavior::
                                  kMoveCursorAfterText);
            std::move(callback).Run(InsertMediaResult::kSuccess);
          },
          [&client, &callback](PickerLocalFileMedia media) mutable {
            if (!client.CanInsertImage()) {
              std::move(callback).Run(InsertMediaResult::kUnsupported);
              return;
            }

            std::optional<std::string> media_type =
                GetMediaTypeFromFilePath(media.path);
            if (!media_type.has_value()) {
              std::move(callback).Run(InsertMediaResult::kUnsupported);
              return;
            }

            ReadFileAsync(
                media.path,
                base::BindOnce(ConvertToDataUrl, std::move(*media_type))
                    .Then(base::BindOnce(
                        [](base::WeakPtr<ui::TextInputClient> client,
                           OnInsertMediaCompleteCallback callback,
                           std::optional<GURL> url) {
                          if (!url.has_value()) {
                            std::move(callback).Run(
                                InsertMediaResult::kNotFound);
                            return;
                          }
                          client->InsertImage(*url);
                          std::move(callback).Run(InsertMediaResult::kSuccess);
                        },
                        client.AsWeakPtr(), std::move(callback))));
          },
      },
      std::move(media));
}

}  // namespace ash
