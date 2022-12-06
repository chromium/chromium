// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_

#include "ui/views/controls/label.h"

namespace ash {

// The text label used by the clipboard history menu.
class ClipboardHistoryLabel : public views::Label {
 public:
  explicit ClipboardHistoryLabel(const std::u16string& text);
  ClipboardHistoryLabel(const ClipboardHistoryLabel& rhs) = delete;
  ClipboardHistoryLabel& operator=(const ClipboardHistoryLabel& rhs) = delete;
  ~ClipboardHistoryLabel() override = default;

  // views::Label:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_LABEL_H_
