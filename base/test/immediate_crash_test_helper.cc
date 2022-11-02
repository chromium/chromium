// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/immediate_crash.h"  // nogncheck

#if defined(WIN32)
#define IMMEDIATE_CRASH_TEST_HELPER_EXPORT __declspec(dllexport)
#else  // defined(WIN32)
#define IMMEDIATE_CRASH_TEST_HELPER_EXPORT \
  __attribute__((visibility("default")))
#endif  // defined(WIN32)

extern "C" {

IMMEDIATE_CRASH_TEST_HELPER_EXPORT int TestFunction1(int x, int y) {
  if (x < 1)
    base::ImmediateCrash();
  if (y < 1)
    base::ImmediateCrash();
  return x + y;
}

IMMEDIATE_CRASH_TEST_HELPER_EXPORT int TestFunction2(int x, int y) {
  if (x < 2)
    base::ImmediateCrash();
  if (y < 2)
    base::ImmediateCrash();
  return x * y;
}

}  // extern "C"
