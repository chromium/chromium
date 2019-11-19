// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRAY_ACTION_TRAY_ACTION_OBSERVER_H_
#define ASH_TRAY_ACTION_TRAY_ACTION_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/tray_action.mojom.h"

namespace ash {

class ASH_EXPORT TrayActionObserver {
 public:
  virtual ~TrayActionObserver() {}

  // Called when action handler state changes.
  virtual void OnLockScreenNoteStateChanged(mojom::TrayActionState state) = 0;
};

}  // namespace ash

#endif  // ASH_TRAY_ACTION_TRAY_ACTION_OBSERVER_H_
