// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class LevelDBPersistedTabDataStorageAndroidFactoryTest : public testing::Test {
 public:
  LevelDBPersistedTabDataStorageAndroidFactoryTest() = default;
  LevelDBPersistedTabDataStorageAndroidFactoryTest(
      const LevelDBPersistedTabDataStorageAndroidFactoryTest&) = delete;
  LevelDBPersistedTabDataStorageAndroidFactoryTest& operator=(
      const LevelDBPersistedTabDataStorageAndroidFactoryTest&) = delete;

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

TEST_F(LevelDBPersistedTabDataStorageAndroidFactoryTest, TestIncognito) {
  EXPECT_EQ(nullptr, LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
                         ->GetForBrowserContext(profile()->GetPrimaryOTRProfile(
                             /*create_if_needed=*/true)));
}

TEST_F(LevelDBPersistedTabDataStorageAndroidFactoryTest, TestSameProfile) {
  EXPECT_EQ(LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
                ->GetForBrowserContext(profile()),
            LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}

TEST_F(LevelDBPersistedTabDataStorageAndroidFactoryTest, TestDifferentProfile) {
  EXPECT_NE(LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
                ->GetForBrowserContext(different_profile()),
            LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}
