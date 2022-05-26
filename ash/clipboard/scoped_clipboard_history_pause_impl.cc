// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"

#include "ash/clipboard/clipboard_history.h"

namespace ash {

ScopedClipboardHistoryPauseImpl::ScopedClipboardHistoryPauseImpl(
    ClipboardHistory* clipboard_history)
    : ScopedClipboardHistoryPauseImpl(clipboard_history,
                                      /*metrics_only=*/false) {}

ScopedClipboardHistoryPauseImpl::ScopedClipboardHistoryPauseImpl(
    ClipboardHistory* clipboard_history,
    bool metrics_only)
    : clipboard_history_(clipboard_history->GetWeakPtr()),
      metrics_only_(metrics_only) {
  clipboard_history_->Pause(metrics_only_);
}

ScopedClipboardHistoryPauseImpl::~ScopedClipboardHistoryPauseImpl() {
  if (clipboard_history_)
    clipboard_history_->Resume(metrics_only_);
}

}  // namespace ash
