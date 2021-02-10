// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_

#include "base/macros.h"

namespace chromeos {
namespace system {

class SystemClock;

// This is observer for chromeos::system::SystemClock .
class SystemClockObserver {
 public:
  virtual ~SystemClockObserver();
  virtual void OnSystemClockChanged(SystemClock*) = 0;
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_
