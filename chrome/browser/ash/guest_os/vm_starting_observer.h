// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_VM_STARTING_OBSERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_VM_STARTING_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {
class VmStartingObserver : public base::CheckedObserver {
 public:
  // Called when the given VM is starting.
  virtual void OnVmStarting() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_GUEST_OS_VM_STARTING_OBSERVER_H_
