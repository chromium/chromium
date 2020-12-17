// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileProtoDBFactoryTest : public testing::Test {
 public:
  ProfileProtoDBFactoryTest() = default;
  ProfileProtoDBFactoryTest(const ProfileProtoDBFactoryTest&) = delete;
  ProfileProtoDBFactoryTest& operator=(const ProfileProtoDBFactoryTest&) =
      delete;

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
};

TEST_F(ProfileProtoDBFactoryTest, TestIncognitoProfile) {
  EXPECT_EQ(nullptr,
            ProfileProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()->GetPrimaryOTRProfile()));
}

TEST_F(ProfileProtoDBFactoryTest, TestSameProfile) {
  EXPECT_EQ(ProfileProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()),
            ProfileProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()));
}

TEST_F(ProfileProtoDBFactoryTest, TestDifferentProfile) {
  EXPECT_NE(ProfileProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(different_profile()),
            ProfileProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()));
}
