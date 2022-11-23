// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_

namespace ash {

// An object that loads a test version of the HelpApp extension for use in
// tests. While this object is in scope `HelpAppLauncher` sends requests to the
// loaded test HelpApp extension. When this object goes out of scope
// `HelpAppLauncher` reverts to sending requests to the production extension.
class ScopedHelpAppForTest {
 public:
  ScopedHelpAppForTest();

  ScopedHelpAppForTest(const ScopedHelpAppForTest&) = delete;
  ScopedHelpAppForTest& operator=(const ScopedHelpAppForTest&) = delete;

  virtual ~ScopedHelpAppForTest();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_HELP_APP_FOR_TEST_H_
