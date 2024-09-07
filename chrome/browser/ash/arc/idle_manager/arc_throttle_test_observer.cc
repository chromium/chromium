// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"

#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace arc::unittest {

void ThrottleTestObserver::Monitor(const ash::ThrottleObserver* target) {
  count_++;
  if (target->active())
    active_count_++;
  if (target->enforced())
    enforced_count_++;
}

}  // namespace arc::unittest
