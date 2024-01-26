// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/clipboard_cleanup_handler.h"

#include <optional>
#include <utility>

#include "ui/base/clipboard/clipboard.h"

namespace chromeos {

ClipboardCleanupHandler::ClipboardCleanupHandler() = default;

ClipboardCleanupHandler::~ClipboardCleanupHandler() = default;

void ClipboardCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  // ChromeOS only has 1 copy/paste clipboard used on all threads.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  std::move(callback).Run(std::nullopt);
}

}  // namespace chromeos
