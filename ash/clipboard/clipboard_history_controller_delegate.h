// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"

namespace ash {

class ClipboardHistoryUrlTitleFetcher;
class ClipboardImageModelFactory;

// The browser-implemented delegate of the `ClipboardHistoryControllerImpl`.
class ASH_EXPORT ClipboardHistoryControllerDelegate {
 public:
  virtual ~ClipboardHistoryControllerDelegate();

  // Creates the browser-implemented URL title fetcher.
  virtual std::unique_ptr<ClipboardHistoryUrlTitleFetcher>
  CreateUrlTitleFetcher() const = 0;

  // Creates the browser-implemented image model factory which renders html.
  virtual std::unique_ptr<ClipboardImageModelFactory> CreateImageModelFactory()
      const = 0;

  // Performs an explicit paste, which is distinct from an implicit paste via
  // a synthetic Ctrl+V event. Returns `true` if successful, otherwise `false`.
  virtual bool Paste() const = 0;

 protected:
  ClipboardHistoryControllerDelegate();
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_H_
