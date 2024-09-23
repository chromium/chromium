// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_VM_SHUTDOWN_OBSERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_VM_SHUTDOWN_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

class VmShutdownObserver : public base::CheckedObserver {
 public:
  // Called when the given VM has shutdown.
  virtual void OnVmShutdown(const std::string& vm_name) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_GUEST_OS_VM_SHUTDOWN_OBSERVER_H_
