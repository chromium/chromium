// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_PROFILE_UMA_H_
#define ASH_MULTI_PROFILE_UMA_H_

#include "ash/ash_export.h"

namespace ash {

// Records UMA statistics for multiprofile actions.
// Note: There is also an action to switch profile windows from the
// browser frame that is recorded by the "Profile.OpenMethod" metric.
class ASH_EXPORT MultiProfileUMA {
 public:
  // Used for UMA metrics. Do not reorder.
  enum class SwitchActiveUserAction {
    kByTray = 0,
    kByAccelerator,
    kNumActions
  };

  MultiProfileUMA() = delete;
  MultiProfileUMA(const MultiProfileUMA&) = delete;
  MultiProfileUMA& operator=(const MultiProfileUMA&) = delete;

  // Record switching the active user and what UI path was taken.
  static void RecordSwitchActiveUser(SwitchActiveUserAction action);
};

}  // namespace ash

#endif  // ASH_MULTI_PROFILE_UMA_H_
