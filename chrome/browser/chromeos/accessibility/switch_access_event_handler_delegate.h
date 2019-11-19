// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_

#include "ash/public/cpp/switch_access_event_handler_delegate.h"
#include "base/macros.h"

namespace ash {
enum class SwitchAccessCommand;
}

// SwitchAccessEventHandlerDelegate receives mouse and key events from Ash's
// event handler and forwards them to the Switch Access extension in Chrome.
class SwitchAccessEventHandlerDelegate
    : public ash::SwitchAccessEventHandlerDelegate {
 public:
  SwitchAccessEventHandlerDelegate();
  virtual ~SwitchAccessEventHandlerDelegate();

 private:
  // ash::SwitchAccessEventHandlerDelegate:
  void SendSwitchAccessCommand(ash::SwitchAccessCommand command) override;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandlerDelegate);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
