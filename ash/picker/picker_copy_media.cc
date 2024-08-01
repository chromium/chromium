// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/check_deref.h"
#include "base/functional/overloaded.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/file_info.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kPickerCopyToClipboardToastId[] = "picker_copy_to_clipboard";

}  // namespace

std::unique_ptr<ui::ClipboardData> ClipboardDataFromMedia(
    const PickerRichMedia& media,
    const PickerClipboardDataOptions& options) {
  auto data = std::make_unique<ui::ClipboardData>();
  std::visit(base::Overloaded{
                 [&data](const PickerTextMedia& media) {
                   data->set_text(base::UTF16ToUTF8(media.text));
                 },
                 [&data, &options](const PickerLinkMedia& media) {
                   std::string escaped_spec =
                       base::EscapeForHTML(media.url.spec());
                   std::string escaped_title = base::EscapeForHTML(media.title);
                   data->set_text(media.url.spec());
                   if (options.links_should_use_title) {
                     data->set_markup_data(
                         base::StrCat({"<a href=\"", escaped_spec, "\">",
                                       escaped_title, "</a>"}));
                   } else {
                     data->set_markup_data(base::StrCat(
                         {"<a title=\"", escaped_title, "\" href=\"",
                          escaped_spec, "\">", escaped_spec, "</a>"}));
                   }
                 },
                 [&data](const PickerLocalFileMedia& media) {
                   data->set_filenames(
                       {ui::FileInfo(media.path, /*display_name=*/{})});
                 },
             },
             media);
  return data;
}

void CopyMediaToClipboard(const PickerRichMedia& media) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(ClipboardDataFromMedia(
          media, PickerClipboardDataOptions{.links_should_use_title = false}));

  // Show a toast to inform the user about the copy.
  // TODO: b/322928125 - Use dedicated toast catalog name.
  // TODO: b/322928125 - Finalize string.
  ToastManager::Get()->Show(ToastData(
      kPickerCopyToClipboardToastId,
      ToastCatalogName::kCopyGifToClipboardAction, u"Copied to clipboard"));
}

}  // namespace ash
