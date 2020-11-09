// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_source.h"

#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/mock_sharing_device_source.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SharingDeviceSourceTest, RunsCallbackImmediatelyIfReady) {
  MockSharingDeviceSource device_source;
  EXPECT_CALL(device_source, IsReady()).WillOnce(testing::Return(true));

  bool did_run_callback = false;
  device_source.AddReadyCallback(base::BindLambdaForTesting(
      [&did_run_callback]() { did_run_callback = true; }));

  EXPECT_TRUE(did_run_callback);
}

TEST(SharingDeviceSourceTest, RunsCallbackAfterIsReady) {
  MockSharingDeviceSource device_source;
  EXPECT_CALL(device_source, IsReady())
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  bool did_run_callback = false;
  device_source.AddReadyCallback(base::BindLambdaForTesting(
      [&did_run_callback]() { did_run_callback = true; }));
  EXPECT_FALSE(did_run_callback);

  device_source.MaybeRunReadyCallbacksForTesting();
  EXPECT_TRUE(did_run_callback);
}
