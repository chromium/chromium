// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_insert_media.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "ash/picker/picker_clipboard_insertion.h"
#include "ash/picker/picker_copy_media.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/picker/picker_web_paste_target.h"
#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "net/base/mime_util.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
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

bool ShouldSkipLinkClipboardInsertion(const GURL& url_of_target) {
  // Google Slides does not correctly handle pasting of links.
  return url_of_target.DomainIs("docs.google.com") &&
         url_of_target.path_piece().starts_with("/presentation/");
}

// Some websites such as https://x.com use a `contenteditable` text field, but
// `<a>` elements are stripped. Inserting
//     <a href="https://example.com">Example</a>
// into these text fields will result in a _plain text_ "Example", without a
// link. As a result, we only insert link titles on a set of allowlisted
// websites. If this returns false, we insert
//     <a title="Example" href="https://example.com">https://example.com</a>
// instead, which, if the `<a>` element is stripped, still inserts the link
// "https://example.com".
//
// TODO: b/337064111 - Determine allowlist for inserting link title.
bool ShouldUseLinkTitle(const GURL& url_of_target) {
  if (url_of_target.DomainIs("google.com")) {
    return !url_of_target.DomainIs("docs.google.com");
  }

  if (url_of_target.DomainIs("onedrive.live.com") ||
      url_of_target.DomainIs("sharepoint.com")) {
    return true;
  }

  return false;
}

void InsertMediaToInputFieldNoClipboard(
    PickerRichMedia media,
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

}  // namespace

bool InputFieldSupportsInsertingMedia(const PickerRichMedia& media,
                                      ui::TextInputClient& client) {
  return std::visit(base::Overloaded{
                        [](const PickerTextMedia& media) { return true; },
                        [&client](const PickerImageMedia& media) {
                          return client.CanInsertImage();
                        },
                        [](const PickerLinkMedia& media) { return true; },
                        [&client](const PickerLocalFileMedia& media) {
                          return client.CanInsertImage();
                        },
                    },
                    media);
}

void InsertMediaToInputField(PickerRichMedia media,
                             ui::TextInputClient& client,
                             WebPasteTargetCallback get_web_paste_target,
                             OnInsertMediaCompleteCallback callback) {
  if (std::holds_alternative<PickerLinkMedia>(media) &&
      client.GetTextInputType() == ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE) {
    std::optional<PickerWebPasteTarget> web_paste_target =
        get_web_paste_target.is_null() ? std::nullopt
                                       : std::move(get_web_paste_target).Run();
    base::OnceClosure do_paste;
    PickerClipboardDataOptions clipboard_data_options;
    bool skip_clipboard_insertion = false;
    if (web_paste_target.has_value()) {
      do_paste = std::move(web_paste_target->do_paste);
      clipboard_data_options.links_should_use_title =
          ShouldUseLinkTitle(web_paste_target->url);
      skip_clipboard_insertion =
          ShouldSkipLinkClipboardInsertion(web_paste_target->url);
    }

    if (!skip_clipboard_insertion) {
      InsertClipboardData(
          ClipboardDataFromMedia(media, clipboard_data_options),
          std::move(do_paste),
          base::BindOnce(
              [](PickerRichMedia media,
                 base::WeakPtr<ui::TextInputClient> client,
                 OnInsertMediaCompleteCallback callback, bool success) {
                if (success) {
                  std::move(callback).Run(InsertMediaResult::kSuccess);
                  return;
                }
                if (client == nullptr) {
                  std::move(callback).Run(InsertMediaResult::kUnsupported);
                  return;
                }
                InsertMediaToInputFieldNoClipboard(std::move(media), *client,
                                                   std::move(callback));
              },
              std::move(media), client.AsWeakPtr(), std::move(callback)));
      return;
    }
  }

  InsertMediaToInputFieldNoClipboard(std::move(media), client,
                                     std::move(callback));
}

}  // namespace ash
