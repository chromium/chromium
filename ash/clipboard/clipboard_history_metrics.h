// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_METRICS_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_METRICS_H_

namespace ash {

// The different operations clipboard history sees. These values are written to
// logs. New enum values can be added, but existing enums must never be
// renumbered, deleted, or reused. Keep this up to date with the
// `ClipboardHistoryOperation` enum in enums.xml.
enum class ClipboardHistoryOperation {
  // Emitted when the user initiates a clipboard write.
  kCopy = 0,

  // Emitted when the user initiates a clipboard read.
  kPaste = 1,

  // Insert new types above this line.
  kMaxValue = kPaste
};

// The different ways a clipboard history list reorder can occur. These values
// are written to logs. New enum values can be added, but existing enums must
// never be renumbered, deleted, or reused. Keep this up to date with the
// `ClipboardHistoryReorderType` enum in enums.xml.
enum class ClipboardHistoryReorderType {
  // Emitted when the user copies an existing clipboard history item other than
  // the top item, causing the copied item to move to the top.
  kOnCopy = 0,

  // Emitted when the user pastes an existing clipboard history item other than
  // the top item, causing the pasted item to move to the top. This behavior is
  // gated by the `kClipboardHistoryReorder` flag.
  kOnPaste = 1,

  // Insert new types above this line.
  kMaxValue = kOnPaste
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_METRICS_H_
