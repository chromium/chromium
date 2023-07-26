// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_url_title_fetcher.h"

#include "base/check_op.h"

namespace ash {

namespace {

ClipboardHistoryUrlTitleFetcher* g_instance = nullptr;

}  // namespace

ClipboardHistoryUrlTitleFetcher::ClipboardHistoryUrlTitleFetcher() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ClipboardHistoryUrlTitleFetcher::~ClipboardHistoryUrlTitleFetcher() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ClipboardHistoryUrlTitleFetcher* ClipboardHistoryUrlTitleFetcher::Get() {
  return g_instance;
}

}  // namespace ash
