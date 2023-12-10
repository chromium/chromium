// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_profile_uma.h"

#include "base/metrics/histogram_macros.h"

namespace ash {

// static
void MultiProfileUMA::RecordSwitchActiveUser(SwitchActiveUserAction action) {
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.SwitchActiveUserUIPath", action,
                             SwitchActiveUserAction::kNumActions);
}

}  // namespace ash
