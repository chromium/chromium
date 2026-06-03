// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_TEST_UTIL_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_TEST_UTIL_H_

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace send_tab_to_self {

// A helper class to wait for a certain number of send-tab-to-self entries to be
// opened. Opening an entry is defined as the entry being marked as opened in
// the SendTabToSelfModel.
class EntryOpenedWaiter : public SendTabToSelfModelObserver {
 public:
  explicit EntryOpenedWaiter(SendTabToSelfModel* model, int expected_count = 1);
  ~EntryOpenedWaiter() override;

  void Wait();

  // SendTabToSelfModelObserver:
  void OnEntriesOpenedRemotely(
      base::span<const SendTabToSelfEntry* const> opened_entries) override;

 private:
  const int expected_count_;
  int current_count_ = 0;
  base::RunLoop run_loop_;
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      observation_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_TEST_UTIL_H_
