// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCOPED_CLIPBOARD_HISTORY_PAUSE_H_
#define ASH_PUBLIC_CPP_SCOPED_CLIPBOARD_HISTORY_PAUSE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An object implemented in Ash that pauses ClipboardHistory for its lifetime.
class ASH_PUBLIC_EXPORT ScopedClipboardHistoryPause {
 public:
  ScopedClipboardHistoryPause() = default;
  virtual ~ScopedClipboardHistoryPause() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCOPED_CLIPBOARD_HISTORY_PAUSE_H_
