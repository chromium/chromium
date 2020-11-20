// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"

#include "ash/clipboard/clipboard_history.h"

namespace ash {

ScopedClipboardHistoryPauseImpl::ScopedClipboardHistoryPauseImpl(
    ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history->GetWeakPtr()) {
  clipboard_history_->Pause();
}

ScopedClipboardHistoryPauseImpl::~ScopedClipboardHistoryPauseImpl() {
  if (clipboard_history_)
    clipboard_history_->Resume();
}

}  // namespace ash
