// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_

#include "chrome/test/base/in_process_browser_test.h"

class Browser;

// A class that provides helper methods for interstitial and safety tips
// lookalike tests.
class LookalikeTestHelper {
 public:
  explicit LookalikeTestHelper(Browser* browser);

  void SetUp();
  void TearDown();

 private:
  Browser* browser_;
};

#endif
