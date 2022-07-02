// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_
#define ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class ClipboardHistory;

// Prevents clipboard history from being recorded within its lifetime. If
// anything is copied within its lifetime, history will not be recorded.
class ASH_EXPORT ScopedClipboardHistoryPauseImpl
    : public ScopedClipboardHistoryPause {
 public:
  explicit ScopedClipboardHistoryPauseImpl(ClipboardHistory* clipboard_history);
  // If `metrics_only` is true, this pause will not prevent modifications to
  // clipboard history, but it will prevent updates to the metrics tracked on
  // clipboard operations.
  ScopedClipboardHistoryPauseImpl(ClipboardHistory* clipboard_history,
                                  bool metrics_only);
  ScopedClipboardHistoryPauseImpl(const ScopedClipboardHistoryPauseImpl&) =
      delete;
  ScopedClipboardHistoryPauseImpl& operator=(
      const ScopedClipboardHistoryPauseImpl&) = delete;
  ~ScopedClipboardHistoryPauseImpl() override;

 private:
  base::WeakPtr<ClipboardHistory> const clipboard_history_;
  const bool metrics_only_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_SCOPED_CLIPBOARD_HISTORY_PAUSE_IMPL_H_
