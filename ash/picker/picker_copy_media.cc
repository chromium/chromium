// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kPickerCopyToClipboardToastId[] = "picker_copy_to_clipboard";

std::string BuildGifHTML(const GURL& url,
                         std::u16string_view content_description) {
  // Referrer-Policy is used to prevent the website from getting information
  // about where the GIFs are being used.
  return base::StringPrintf(
      R"html(<img src="%s" referrerpolicy="no-referrer" alt="%s"/>)html",
      url.spec().c_str(),
      base::EscapeForHTML(base::UTF16ToUTF8(content_description)).c_str());
}

}  // namespace

void CopyGifMediaToClipboard(const GURL& url,
                             std::u16string_view content_description) {
  // Overwrite the clipboard data with the GIF url.
  auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste);

  clipboard->WriteHTML(
      base::UTF8ToUTF16(BuildGifHTML(url, content_description)),
      /*document_url=*/"");

  // Show a toast to inform the user about the copy.
  // TODO: b/322928125 - Use dedicated toast catalog name.
  // TODO: b/322928125 - Finalize string.
  ToastManager::Get()->Show(ToastData(
      kPickerCopyToClipboardToastId,
      ToastCatalogName::kCopyGifToClipboardAction, u"Copied to clipboard"));
}

}  // namespace ash
