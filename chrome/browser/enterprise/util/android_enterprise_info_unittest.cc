// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/android_enterprise_info.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

class AndroidEnterpriseInfoTest : public ::testing::Test {
 protected:
  AndroidEnterpriseInfoTest() {
    instance_ = enterprise_util::AndroidEnterpriseInfo::GetInstance();
    // Java side isn't running, so we need to skip the call to it.
    instance_->set_skip_jni_call_for_testing(true);
  }

  raw_ptr<enterprise_util::AndroidEnterpriseInfo> instance_;
};

class EnterpriseInfoCallbackHelper {
 public:
  std::optional<bool> is_profile_owned, is_device_owned;
  int num_times_called = 0;

  void OnResult(bool profile_owned, bool device_owned) {
    is_profile_owned = profile_owned;
    is_device_owned = device_owned;
    num_times_called++;
  }
};

// Test that a callback can be queued and receives the correct results when
// serviced.
TEST_F(AndroidEnterpriseInfoTest, CallbackGetsResult) {
  EnterpriseInfoCallbackHelper helper;

  // Add a callback to the queue.
  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));

  bool profile_value = true;
  bool device_value = false;
  // Now pretend we got a result.
  instance_->ServiceCallbacksForTesting(profile_value, device_value);

  ASSERT_TRUE(helper.is_profile_owned.has_value());
  ASSERT_TRUE(helper.is_device_owned.has_value());
  EXPECT_EQ(helper.is_profile_owned, profile_value);
  EXPECT_EQ(helper.is_device_owned, device_value);
}

// Test that multiple queued callbacks are all serviced.
TEST_F(AndroidEnterpriseInfoTest, MultipleCallbacksServiced) {
  EnterpriseInfoCallbackHelper helper;

  const int kCOUNT = 6;
  for (int i = 0; i < kCOUNT; i++) {
    instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
        &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));
  }

  // Nothing should get called until the callbacks are serviced.
  EXPECT_EQ(helper.num_times_called, 0);
  // Now pretend we got a result. Value is irrelevant in this test.
  instance_->ServiceCallbacksForTesting(false, false);

  EXPECT_EQ(helper.num_times_called, kCOUNT);
}

// Test that callbacks are still serviced after prior callbacks have already
// been serviced.
TEST_F(AndroidEnterpriseInfoTest, MultipleCallbacksNonconcurrentServiced) {
  EnterpriseInfoCallbackHelper prehelper;

  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&prehelper)));
  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&prehelper)));

  // Now pretend we got a result. Value is irrelevant in this test.
  instance_->ServiceCallbacksForTesting(false, false);

  // After servicing, load more callbacks.
  EnterpriseInfoCallbackHelper helper;

  const int kCOUNT = 4;
  for (int i = 0; i < kCOUNT; i++) {
    instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
        &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));
  }

  EXPECT_EQ(helper.num_times_called, 0);
  // Now pretend we got a result. Value is irrelevant in this test, but let's
  // change this one just to highlight that you can't assume it stays the same
  // between servicings.
  instance_->ServiceCallbacksForTesting(true, true);

  EXPECT_EQ(helper.num_times_called, kCOUNT);
}

// Test that multiple callbacks all receive the same result.
TEST_F(AndroidEnterpriseInfoTest, MultipleCallbacksSameResult) {
  EnterpriseInfoCallbackHelper helper1, helper2;

  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper1)));
  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper2)));

  bool profile_value = false;
  bool device_value = true;
  // Now pretend we got a result.
  instance_->ServiceCallbacksForTesting(profile_value, device_value);

  // If at least one optional has a value, then the other optional can only be
  // equivalent if it has the same value.
  ASSERT_TRUE(helper2.is_profile_owned.has_value());
  ASSERT_TRUE(helper2.is_device_owned.has_value());
  EXPECT_EQ(helper1.is_profile_owned, helper2.is_profile_owned);
  EXPECT_EQ(helper1.is_device_owned, helper2.is_device_owned);
}

// Test that a reentrant callback doesn't cause a loop.
TEST_F(AndroidEnterpriseInfoTest, ReentrantCallback) {
  EnterpriseInfoCallbackHelper helper;

  // Insert 4 callbacks into the instance, the 2nd callback will attempt to
  // insert another callback when it's serviced.
  enterprise_util::AndroidEnterpriseInfo::EnterpriseInfoCallback
      reentrant_callback =
          base::BindLambdaForTesting([&](bool one, bool two) -> void {
            // Do nothing with the arguments, just enter the function again.
            instance_->GetAndroidEnterpriseInfoState(
                base::BindOnce(&EnterpriseInfoCallbackHelper::OnResult,
                               base::Unretained(&helper)));
          });

  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));
  instance_->GetAndroidEnterpriseInfoState(std::move(reentrant_callback));
  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));
  instance_->GetAndroidEnterpriseInfoState(base::BindOnce(
      &EnterpriseInfoCallbackHelper::OnResult, base::Unretained(&helper)));

  // Nothing should get called until the callbacks are serviced.
  EXPECT_EQ(helper.num_times_called, 0);
  // Now pretend we got a result. Value is irrelevant in this test.
  instance_->ServiceCallbacksForTesting(false, false);

  // Only the first 3 |helper| calls should have been serviced at this point.
  // The last one should be waiting in the (new) callback queue.
  EXPECT_EQ(helper.num_times_called, 3);

  // Service callbacks again to get the fourth one.
  // Value is irrelevant, but mix it up anyway.
  instance_->ServiceCallbacksForTesting(true, false);
  EXPECT_EQ(helper.num_times_called, 4);
}
