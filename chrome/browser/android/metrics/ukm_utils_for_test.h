// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_UKM_UTILS_FOR_TEST_H_
#define CHROME_BROWSER_ANDROID_METRICS_UKM_UTILS_FOR_TEST_H_

#include <stdint.h>

#include "base/macros.h"

namespace ukm {

// The native part of java UkmUtilsForTest class.
class UkmUtilsForTest {
 public:
  static bool IsEnabled();
  static bool HasSourceWithId(SourceId source_id);
  static void RecordSourceWithId(SourceId source_id);
  static uint64_t GetClientId();

 private:
  // Should never be needed, as this class is setup to let it be a friend to
  // access UKM internals for testing.
  UkmUtilsForTest();
  ~UkmUtilsForTest();

  DISALLOW_COPY_AND_ASSIGN(UkmUtilsForTest);
};

}  // namespace ukm

#endif  // CHROME_BROWSER_ANDROID_METRICS_UKM_UTILS_FOR_TEST_H_
