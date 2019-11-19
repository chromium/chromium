// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_

#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"
#include "base/macros.h"

namespace chromeos {

// SelectToSpeakEventHandlerDelegate receives mouse and key events from Ash's
// event handler and forwards them to the Select-to-Speak extension in Chrome.
class SelectToSpeakEventHandlerDelegate
    : public ash::SelectToSpeakEventHandlerDelegate {
 public:
  SelectToSpeakEventHandlerDelegate();
  virtual ~SelectToSpeakEventHandlerDelegate();

 private:
  // ash::SelectToSpeakEventHandlerDelegate:
  void DispatchKeyEvent(const ui::KeyEvent& event) override;
  void DispatchMouseEvent(const ui::MouseEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventHandlerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_H_
