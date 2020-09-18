// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/state/tab_state_db_factory.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabStateDBFactoryTest : public testing::Test {
 public:
  TabStateDBFactoryTest() = default;

  void SetUp() override {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(different_profile_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_dir_.GetPath());
    profile_ = profile_builder.Build();

    TestingProfile::Builder different_profile_builder;
    different_profile_builder.SetPath(different_profile_dir_.GetPath());
    different_profile_ = different_profile_builder.Build();
  }

  Profile* profile() { return profile_.get(); }
  Profile* different_profile() { return different_profile_.get(); }

 private:
  base::ScopedTempDir profile_dir_;
  base::ScopedTempDir different_profile_dir_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfile> different_profile_;

  DISALLOW_COPY_AND_ASSIGN(TabStateDBFactoryTest);
};

TEST_F(TabStateDBFactoryTest, TestIncognitoProfile) {
  EXPECT_EQ(nullptr, TabStateDBFactory::GetInstance()->GetForProfile(
                         profile()->GetPrimaryOTRProfile()));
}

TEST_F(TabStateDBFactoryTest, TestSameProfile) {
  EXPECT_EQ(TabStateDBFactory::GetInstance()->GetForProfile(profile()),
            TabStateDBFactory::GetInstance()->GetForProfile(profile()));
}

TEST_F(TabStateDBFactoryTest, TestDifferentProfile) {
  EXPECT_NE(
      TabStateDBFactory::GetInstance()->GetForProfile(different_profile()),
      TabStateDBFactory::GetInstance()->GetForProfile(profile()));
}
