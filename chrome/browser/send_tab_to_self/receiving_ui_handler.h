// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_

#include <string>

#include "base/containers/span.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Interface implemented by platforms to handle changes to the SendTabToSelf
// model. Implementors of this interface should override all functions and
// update the UI accordingly. They should also register themselves with the
// ReceivingUIRegistry.
// TODO(crbug.com/519101926): Consider moving this interface to
// components/send_tab_to_self to allow iOS to use it and unify the receiving
// flow.
class ReceivingUiHandler {
 public:
  ReceivingUiHandler() = default;
  virtual ~ReceivingUiHandler() = default;

  // Display the new entries passed in as an argument. The entries are owned by
  // the model and should not be modified.
  virtual void DisplayNewEntries(
      base::span<const SendTabToSelfEntry* const> new_entries) = 0;
  // Dismiss any UI associated with this entry.
  // Entry object is owned by the the model and should not be
  // modified by any implementors of this class.
  virtual void DismissEntries(base::span<const std::string> guids) = 0;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_
