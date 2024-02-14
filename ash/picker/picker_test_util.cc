// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_test_util.h"

#include <string>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"

namespace ash {

std::u16string ReadHtmlFromClipboard(ui::Clipboard* clipboard) {
  std::u16string data;
  std::string url;
  uint32_t fragment_start, fragment_end;

  clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, nullptr, &data, &url,
                      &fragment_start, &fragment_end);
  return data;
}

}  // namespace ash
