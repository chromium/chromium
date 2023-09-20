// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// TODO(http://b/301264185): Create image model factory and URL title fetcher.
// The browser-implemented delegate of the `ClipboardHistoryControllerImpl`.
class ASH_EXPORT ClipboardHistoryControllerDelegate {
 public:
  virtual ~ClipboardHistoryControllerDelegate();

 protected:
  ClipboardHistoryControllerDelegate();
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_
