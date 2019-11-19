// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_ACTIVATION_OBSERVER_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_ACTIVATION_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// Interface for ash to notify client of sessions status for a specific
// |account_id|.
class ASH_PUBLIC_EXPORT SessionActivationObserver
    : public base::CheckedObserver {
 public:
  // Called the session is becoming active or inactive.
  virtual void OnSessionActivated(bool activated) = 0;

  // Called when lock screen state changes.
  virtual void OnLockStateChanged(bool locked) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_ACTIVATION_OBSERVER_H_
