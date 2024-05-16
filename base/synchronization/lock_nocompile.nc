// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/synchronization/lock.h"

namespace base {

class SynchronizedInt {
public:
  void ResetWithoutCheckingAutolock() {
    AutoTryLock maybe(lock);
    value = 0; // expected-error {{writing variable 'value' requires holding mutex 'lock' exclusively}}
  }

private:
  Lock lock;
  int value GUARDED_BY(lock) = 0;
};

}  // namespace base
