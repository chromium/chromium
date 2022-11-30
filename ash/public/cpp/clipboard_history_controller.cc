// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/clipboard_history_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
ClipboardHistoryController* g_instance = nullptr;
}  // namespace

ClipboardHistoryController::ClipboardHistoryController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ClipboardHistoryController::~ClipboardHistoryController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ClipboardHistoryController* ClipboardHistoryController::Get() {
  return g_instance;
}

}  // namespace ash
