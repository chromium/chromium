// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_

#include "base/macros.h"

namespace chromeos {

// An object that loads a test version of the HelpApp extension for use in
// tests. While this object is in scope |HelpAppLauncher| sends requests to the
// loaded test HelpApp extension. When this object goes out of scope
// |HelpAppLauncher| reverts to sending requests to the production extension.
class ScopedHelpAppForTest {
 public:
  ScopedHelpAppForTest();
  virtual ~ScopedHelpAppForTest();

  DISALLOW_COPY_AND_ASSIGN(ScopedHelpAppForTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_
