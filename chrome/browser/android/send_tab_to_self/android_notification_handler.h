// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Responsible for displaying notifications on Android. Overrides
// ReceivingUIHandler so that it is called for all updates to share
// entries.
class AndroidNotificationHandler : public ReceivingUiHandler {
 public:
  ~AndroidNotificationHandler() override = default;

 private:
  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
