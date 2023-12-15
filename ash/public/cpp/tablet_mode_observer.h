// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Used to observe tablet mode changes inside ash. Exported for tests.
// Note: If you want to observe the tablet mode change on display, use
// display::DisplayObserver::OnDisplayTabletStateChanged().
class ASH_PUBLIC_EXPORT TabletModeObserver {
 public:
  // Called when tablet mode blocks or unblocks events. This usually matches,
  // exiting or entering tablet mode, except when an external mouse is
  // connected.
  virtual void OnTabletModeEventsBlockingChanged() {}

  // Called when the tablet mode controller is destroyed, to help manage issues
  // with observers being destroyed after controllers.
  virtual void OnTabletControllerDestroyed() {}

  // Called when the tablet physical state of the device changes (e.g. due to
  // lid angle changes, device attached/detached from base, ... etc.). It's
  // called before any notifications of UI changes (such as
  // display::DisplayObserver::OnDisplayTabletStateChanged etc.) that are
  // results of this physical state change.
  virtual void OnTabletPhysicalStateChanged() {}

 protected:
  virtual ~TabletModeObserver() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_OBSERVER_H_
