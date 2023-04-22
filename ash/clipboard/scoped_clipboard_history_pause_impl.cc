// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_util.h"

namespace ash {

using PauseBehavior = clipboard_history_util::PauseBehavior;

ScopedClipboardHistoryPauseImpl::ScopedClipboardHistoryPauseImpl(
    ClipboardHistory* clipboard_history)
    : ScopedClipboardHistoryPauseImpl(clipboard_history,
                                      PauseBehavior::kDefault) {}

ScopedClipboardHistoryPauseImpl::ScopedClipboardHistoryPauseImpl(
    ClipboardHistory* clipboard_history,
    PauseBehavior pause_behavior)
    : pause_id_(clipboard_history->Pause(pause_behavior)),
      clipboard_history_(clipboard_history->GetWeakPtr()) {}

ScopedClipboardHistoryPauseImpl::~ScopedClipboardHistoryPauseImpl() {
  if (clipboard_history_)
    clipboard_history_->Resume(*pause_id_);
}

}  // namespace ash
