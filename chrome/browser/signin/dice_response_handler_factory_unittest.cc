// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the DiceResponseHandler is created for a normal profile but not
// for off-the-record profiles.
TEST(DiceResponseHandlerFactoryTest, NotInOffTheRecord) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_THAT(DiceResponseHandlerFactory::GetForProfile(&profile),
              testing::NotNull());
  EXPECT_THAT(DiceResponseHandlerFactory::GetForProfile(
                  profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)),
              testing::IsNull());
  EXPECT_THAT(
      DiceResponseHandlerFactory::GetForProfile(profile.GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)),
      testing::IsNull());
}
