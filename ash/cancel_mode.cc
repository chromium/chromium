// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/cancel_mode.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"

namespace ash {

void DispatchCancelMode() {
  Shell::RootWindowControllerList controllers(
      Shell::GetAllRootWindowControllers());
  for (Shell::RootWindowControllerList::const_iterator i = controllers.begin();
       i != controllers.end(); ++i) {
    (*i)->GetHost()->dispatcher()->DispatchCancelModeEvent();
  }
}

}  // namespace ash
