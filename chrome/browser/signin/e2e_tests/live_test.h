// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_

#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace signin {
namespace test {

class LiveTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUp() override;
  void TearDown() override;
  void PostRunTestOnMainThread() override;

  const TestAccountsUtil* GetTestAccountsUtil() const {
    return &test_accounts_;
  }

 private:
  TestAccountsUtil test_accounts_;
  bool skip_test_ = false;
};

}  // namespace test
}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_
