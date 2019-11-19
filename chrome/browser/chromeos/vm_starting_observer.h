// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VM_STARTING_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_VM_STARTING_OBSERVER_H_

#include "base/observer_list_types.h"

namespace chromeos {
class VmStartingObserver : public base::CheckedObserver {
 public:
  // Called when the given VM is starting.
  virtual void OnVmStarting() = 0;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VM_STARTING_OBSERVER_H_
