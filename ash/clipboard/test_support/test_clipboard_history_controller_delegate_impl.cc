// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/test_support/test_clipboard_history_controller_delegate_impl.h"

namespace ash {

TestClipboardHistoryControllerDelegateImpl::
    TestClipboardHistoryControllerDelegateImpl() = default;

TestClipboardHistoryControllerDelegateImpl::
    ~TestClipboardHistoryControllerDelegateImpl() = default;

std::unique_ptr<ClipboardHistoryUrlTitleFetcher>
TestClipboardHistoryControllerDelegateImpl::CreateUrlTitleFetcher() const {
  return nullptr;
}

std::unique_ptr<ClipboardImageModelFactory>
TestClipboardHistoryControllerDelegateImpl::CreateImageModelFactory() const {
  return nullptr;
}

bool TestClipboardHistoryControllerDelegateImpl::Paste() const {
  return false;
}

}  // namespace ash
