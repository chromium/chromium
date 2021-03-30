// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_metrics_utils.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninMetricsUtilsTest : public ::testing::Test {
 public:
  SigninMetricsUtilsTest() = default;
  ~SigninMetricsUtilsTest() override = default;

  SigninMetricsUtilsTest(const SigninMetricsUtilsTest&) = delete;
  SigninMetricsUtilsTest& operator=(const SigninMetricsUtilsTest&) = delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::vector<std::string> gaia_ids_{"default_account", "non_default_account"};
};

TEST_F(SigninMetricsUtilsTest, WebSignInDefaultAccount) {
  base::HistogramTester histogram_tester;
  identity_test_env_.MakeAccountAvailableWithCookies("defaultAccount@gmail.com",
                                                     gaia_ids_[0]);

  signin_metrics_utils::LogWebSignin(identity_test_env_.identity_manager(),
                                     gaia_ids_);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAfterDismissal", 0, 1);
}

TEST_F(SigninMetricsUtilsTest, WebSignInNonDefaultAccount) {
  base::HistogramTester histogram_tester;
  identity_test_env_.MakeAccountAvailableWithCookies(
      "nonDefaultAccount@gmail.com", gaia_ids_[1]);

  signin_metrics_utils::LogWebSignin(identity_test_env_.identity_manager(),
                                     gaia_ids_);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAfterDismissal", 1, 1);
}

TEST_F(SigninMetricsUtilsTest, WebSignInOtherAccount) {
  base::HistogramTester histogram_tester;
  identity_test_env_.MakeAccountAvailableWithCookies("otherAccount@gmail.com",
                                                     "other_account");

  signin_metrics_utils::LogWebSignin(identity_test_env_.identity_manager(),
                                     gaia_ids_);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountConsistencyPromoAfterDismissal", 2, 1);
}

TEST_F(SigninMetricsUtilsTest, MetricsNotRecordedWhenPrimaryAccountSet) {
  base::HistogramTester histogram_tester;
  identity_test_env_.MakeAccountAvailableWithCookies("defaultAccount@gmail.com",
                                                     gaia_ids_[0]);
  identity_test_env_.SetPrimaryAccount("defaultAccount@gmail.com");

  signin_metrics_utils::LogWebSignin(identity_test_env_.identity_manager(),
                                     gaia_ids_);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAfterDismissal", 0);
}

TEST_F(SigninMetricsUtilsTest, MetricsNotRecordedWithoutWebSignIn) {
  base::HistogramTester histogram_tester;

  signin_metrics_utils::LogWebSignin(identity_test_env_.identity_manager(),
                                     gaia_ids_);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAfterDismissal", 0);
}
