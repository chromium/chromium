// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_TAB_HANDLER_H_
#define ASH_SHELL_TAB_HANDLER_H_

#include "ui/events/event_handler.h"

namespace ash {

class Shell;

// Enables handling of tab when there are no non-minimized windows open in the
// shell. This allows keyboard only users to easily get focus to the shelf when
// no windows are open.
class ShellTabHandler : public ui::EventHandler {
 public:
  explicit ShellTabHandler(Shell* shell) : shell_(shell) {}
  ShellTabHandler(const ShellTabHandler&) = delete;
  ShellTabHandler& operator=(const ShellTabHandler) = delete;
  ~ShellTabHandler() override = default;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key_event) override;

 private:
  Shell* const shell_;
};

}  // namespace ash
#endif  // ASH_SHELL_TAB_HANDLER_H_
