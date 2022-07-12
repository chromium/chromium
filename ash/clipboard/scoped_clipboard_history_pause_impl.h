// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_
#define ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "base/memory/weak_ptr.h"
#include "base/token.h"

namespace ash {
class ClipboardHistory;

// Controls modifications to clipboard history within its lifetime. If clipboard
// data is read or modified within its lifetime, the individual pause's behavior
// dictates whether clipboard history and corresponding metrics will be updated.
class ASH_EXPORT ScopedClipboardHistoryPauseImpl
    : public ScopedClipboardHistoryPause {
 public:
  explicit ScopedClipboardHistoryPauseImpl(ClipboardHistory* clipboard_history);
  ScopedClipboardHistoryPauseImpl(ClipboardHistory* clipboard_history,
                                  ClipboardHistoryUtil::PauseBehavior behavior);
  ScopedClipboardHistoryPauseImpl(const ScopedClipboardHistoryPauseImpl&) =
      delete;
  ScopedClipboardHistoryPauseImpl& operator=(
      const ScopedClipboardHistoryPauseImpl&) = delete;
  ~ScopedClipboardHistoryPauseImpl() override;

 private:
  const base::Token& pause_id_;
  base::WeakPtr<ClipboardHistory> const clipboard_history_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_
