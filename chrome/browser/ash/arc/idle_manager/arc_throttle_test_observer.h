// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_THROTTLE_TEST_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_THROTTLE_TEST_OBSERVER_H_

namespace ash {
class ThrottleObserver;
}

namespace arc::unittest {

// Utility to monitor the behavior of a ThrottleObserver object.
// Usage: bind this object's Monitor() method to the callback of an observer
// when you do StartObserving on the object under test, then watch
// the internal counters change.
class ThrottleTestObserver {
 public:
  ThrottleTestObserver() = default;
  ~ThrottleTestObserver() = default;

  // Total number of invocations of the Monitor() method.
  int count() const { return count_; }

  // Number of invocations of the Monitor() method while |target| is
  // in active state.
  int active_count() const { return active_count_; }

  // Number of invocations of the Monitor() method while |target| is
  // in enforced state.
  int enforced_count() const { return enforced_count_; }

  // Bumps internal counters depending on the state of |target|.
  void Monitor(const ash::ThrottleObserver* target);

 private:
  int count_ = 0;
  int active_count_ = 0;
  int enforced_count_ = 0;
};

}  // namespace arc::unittest

#endif  // CHROME_BROWSER_ASH_ARC_IDLE_MANAGER_ARC_THROTTLE_TEST_OBSERVER_H_
