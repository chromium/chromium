// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SessionProtoDBFactoryTest : public testing::Test {
 public:
  SessionProtoDBFactoryTest() = default;
  SessionProtoDBFactoryTest(const SessionProtoDBFactoryTest&) = delete;
  SessionProtoDBFactoryTest& operator=(const SessionProtoDBFactoryTest&) =
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

TEST_F(SessionProtoDBFactoryTest, TestIncognitoProfile) {
  EXPECT_EQ(nullptr,
            SessionProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()->GetPrimaryOTRProfile(
                    /*create_if_needed=*/true)));
}

TEST_F(SessionProtoDBFactoryTest, TestSameProfile) {
  EXPECT_EQ(SessionProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()),
            SessionProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()));
}

TEST_F(SessionProtoDBFactoryTest, TestDifferentProfile) {
  EXPECT_NE(SessionProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(different_profile()),
            SessionProtoDBFactory<
                persisted_state_db::PersistedStateContentProto>::GetInstance()
                ->GetForProfile(profile()));
}
