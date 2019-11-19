// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

class MediaHistoryKeyedServiceFactoryUnitTest : public testing::Test {
 public:
  MediaHistoryKeyedServiceFactoryUnitTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MediaHistoryKeyedServiceFactoryUnitTest, GetForOTRProfile) {
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();

  EXPECT_EQ(nullptr,
            MediaHistoryKeyedServiceFactory::GetForProfile(otr_profile));
}

}  // namespace media_history
