// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_test_util.h"

#include <string>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

std::u16string ReadTextFromClipboard(ui::Clipboard* clipboard) {
  std::u16string data;

  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, nullptr, &data);
  return data;
}

std::u16string ReadHtmlFromClipboard(ui::Clipboard* clipboard) {
  std::u16string data;
  std::string url;
  uint32_t fragment_start, fragment_end;

  clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, nullptr, &data, &url,
                      &fragment_start, &fragment_end);
  return data;
}

base::FilePath ReadFilenameFromClipboard(ui::Clipboard* clipboard) {
  std::vector<ui::FileInfo> result;
  clipboard->ReadFilenames(ui::ClipboardBuffer::kCopyPaste, nullptr, &result);
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
