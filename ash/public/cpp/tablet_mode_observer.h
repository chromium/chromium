// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Used to observe tablet mode changes inside ash. Exported for tests.
class ASH_PUBLIC_EXPORT TabletModeObserver {
 public:
  // Called when the tablet mode is about to start.
  virtual void OnTabletModeStarting() {}

  // Called when the tablet mode has started. Windows might still be animating
  // though.
  virtual void OnTabletModeStarted() {}

  // Called when the tablet mode is about to end.
  virtual void OnTabletModeEnding() {}

  // Called when the tablet mode has ended. Windows may still be animating but
  // have been restored.
  virtual void OnTabletModeEnded() {}

  // Called when tablet mode blocks or unblocks events. This usually matches,
  // exiting or entering tablet mode, except when an external mouse is
  // connected.
  virtual void OnTabletModeEventsBlockingChanged() {}

  // Called when the tablet mode controller is destroyed, to help manage issues
  // with observers being destroyed after controllers.
  virtual void OnTabletControllerDestroyed() {}

  // Called when the tablet physical state of the device changes (e.g. due to
  // lid angle changes, device attached/detached from base, ... etc.). It's
  // called before any notifications of UI changes (such as OnTabletModeStarted,
  // OnTabletModeEnded, ... etc.) that are results of this physical state
  // change.
  virtual void OnTabletPhysicalStateChanged() {}

 protected:
  virtual ~TabletModeObserver() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_
