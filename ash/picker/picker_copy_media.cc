// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/functional/overloaded.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kPickerCopyToClipboardToastId[] = "picker_copy_to_clipboard";

}  // namespace

void CopyMediaToClipboard(const PickerRichMedia& media) {
  auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste);

  // Overwrite the clipboard data.
  std::visit(
      base::Overloaded{
          [&clipboard](const PickerTextMedia& media) {
            clipboard->WriteText(std::move(media.text));
          },
          [&clipboard](const PickerLinkMedia& media) {
            // TODO(b/322729192): Copy a real hyperlink.
            clipboard->WriteText(base::UTF8ToUTF16(media.url.spec()));
          },
          [&clipboard](const PickerLocalFileMedia& media) {
            clipboard->WriteFilenames(ui::FileInfosToURIList(
                /*filenames=*/{ui::FileInfo(media.path, /*display_name=*/{})}));
          },
      },
      media);

  // Show a toast to inform the user about the copy.
  // TODO: b/322928125 - Use dedicated toast catalog name.
  // TODO: b/322928125 - Finalize string.
  ToastManager::Get()->Show(ToastData(
      kPickerCopyToClipboardToastId,
      ToastCatalogName::kCopyGifToClipboardAction, u"Copied to clipboard"));
}

}  // namespace ash
