// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler_test_util.h"

#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace send_tab_to_self {

EntryOpenedWaiter::EntryOpenedWaiter(SendTabToSelfModel* model,
                                     int expected_count)
    : expected_count_(expected_count) {
  observation_.Observe(model);
}

EntryOpenedWaiter::~EntryOpenedWaiter() = default;

void EntryOpenedWaiter::Wait() {
  run_loop_.Run();
}

void EntryOpenedWaiter::OnEntriesOpenedRemotely(
    base::span<const SendTabToSelfEntry* const> opened_entries) {
  current_count_ += opened_entries.size();
  if (current_count_ >= expected_count_) {
    run_loop_.Quit();
  }
}

}  // namespace send_tab_to_self
