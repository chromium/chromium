// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_
#define ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT MultiUserWindowManagerObserver {
 public:
  // Invoked when the user switch animation is finished.
  virtual void OnUserSwitchAnimationFinished() {}

 protected:
  virtual ~MultiUserWindowManagerObserver() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MULTI_USER_WINDOW_MANAGER_OBSERVER_H_
