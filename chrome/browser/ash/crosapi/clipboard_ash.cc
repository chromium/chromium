// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/clipboard_ash.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"

namespace crosapi {

ClipboardAsh::ClipboardAsh() = default;
ClipboardAsh::~ClipboardAsh() = default;

void ClipboardAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Clipboard> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ClipboardAsh::GetCopyPasteText(GetCopyPasteTextCallback callback) {
  std::u16string text;

  const ui::DataTransferEndpoint endpoint(ui::EndpointType::kLacros);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &endpoint, &text);

  std::move(callback).Run(base::UTF16ToUTF8(text));
}

}  // namespace crosapi
