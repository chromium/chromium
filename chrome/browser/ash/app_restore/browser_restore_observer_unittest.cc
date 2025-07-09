// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/browser_restore_observer.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class BrowserRestoreObserverTest : public testing::Test {
 public:
  BrowserRestoreObserverTest() = default;
  BrowserRestoreObserverTest(const BrowserRestoreObserverTest&) = delete;
  BrowserRestoreObserverTest& operator=(const BrowserRestoreObserverTest&) =
      delete;
  ~BrowserRestoreObserverTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowserRestoreObserverTest, RestoresURLsForRegularProfiles) {
  EXPECT_TRUE(
      BrowserRestoreObserver::CanRestoreUrlsForProfileForTesting(&profile_));
}

TEST_F(BrowserRestoreObserverTest, DoesNotRestoreURLsForIncognitoProfiles) {
  Profile* incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(BrowserRestoreObserver::CanRestoreUrlsForProfileForTesting(
      incognito_profile));
}

}  // namespace ash
