// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_TEST_SUPPORT_TEST_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
#define ASH_CLIPBOARD_TEST_SUPPORT_TEST_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history_controller_delegate.h"

namespace ash {

// The test-implemented delegate of the `ClipboardHistoryControllerImpl`.
class ASH_EXPORT TestClipboardHistoryControllerDelegateImpl
    : public ClipboardHistoryControllerDelegate {
 public:
  TestClipboardHistoryControllerDelegateImpl();
  TestClipboardHistoryControllerDelegateImpl(
      const TestClipboardHistoryControllerDelegateImpl&) = delete;
  TestClipboardHistoryControllerDelegateImpl& operator=(
      const TestClipboardHistoryControllerDelegateImpl&) = delete;
  ~TestClipboardHistoryControllerDelegateImpl() override;

 private:
  // ClipboardHistoryControllerDelegate:
  std::unique_ptr<ClipboardHistoryUrlTitleFetcher> CreateUrlTitleFetcher()
      const override;
  std::unique_ptr<ClipboardImageModelFactory> CreateImageModelFactory()
      const override;
  bool Paste() const override;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_TEST_SUPPORT_TEST_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
