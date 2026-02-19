// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_test_util.h"

#include <string>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

std::u16string ReadTextFromClipboard(ui::Clipboard* clipboard) {
  return ui::clipboard_test_util::ReadText(clipboard,
                                           ui::ClipboardBuffer::kCopyPaste,
                                           /*data_dst=*/nullptr);
}

std::u16string ReadHtmlFromClipboard(ui::Clipboard* clipboard) {
  std::u16string data;
  std::string url;
  uint32_t fragment_start, fragment_end;

  ui::clipboard_test_util::ReadHTML(clipboard, ui::ClipboardBuffer::kCopyPaste,
                                    /*data_dst=*/nullptr, &data, &url,
                                    &fragment_start, &fragment_end);
  return data;
}

base::FilePath ReadFilenameFromClipboard(ui::Clipboard* clipboard) {
  std::vector<ui::FileInfo> result = ui::clipboard_test_util::ReadFilenames(
      clipboard, ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr);
  return result.empty() ? base::FilePath() : result.front().path;
}

void LeftClickOn(views::View& view) {
  ui::test::EventGenerator event_generator(GetRootWindow(view.GetWidget()));
  event_generator.MoveMouseTo(view.GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

void PressAndReleaseKey(views::Widget& widget,
                        ui::KeyboardCode key_code,
                        int flags) {
  ui::test::EventGenerator event_generator(GetRootWindow(&widget));
  event_generator.PressAndReleaseKey(key_code, flags);
}

}  // namespace ash
