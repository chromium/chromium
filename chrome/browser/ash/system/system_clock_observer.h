// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_
#define CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_

namespace ash {
namespace system {

class SystemClock;

// This is observer for ash::system::SystemClock .
class SystemClockObserver {
 public:
  virtual ~SystemClockObserver();
  virtual void OnSystemClockChanged(SystemClock*) = 0;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_OBSERVER_H_
