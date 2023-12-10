// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// The text label used by the clipboard history menu.
class ClipboardHistoryLabel : public views::Label {
  METADATA_HEADER(ClipboardHistoryLabel, views::Label)

 public:
  ClipboardHistoryLabel(const std::u16string& text,
                        gfx::ElideBehavior elide_behavior,
                        size_t max_lines);

  ClipboardHistoryLabel(const ClipboardHistoryLabel& rhs) = delete;
  ClipboardHistoryLabel& operator=(const ClipboardHistoryLabel& rhs) = delete;
  ~ClipboardHistoryLabel() override = default;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_
