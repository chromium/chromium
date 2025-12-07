// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/features.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_experience::features {

class TestFieldTrialObserver : public base::FieldTrialList::Observer {
 public:
  TestFieldTrialObserver() { base::FieldTrialList::AddObserver(this); }
  ~TestFieldTrialObserver() override {
    base::FieldTrialList::RemoveObserver(this);
  }

  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override {
    finalized_trials_[trial.trial_name()] = group_name;
  }

  const std::map<std::string, std::string>& finalized_trials() const {
    return finalized_trials_;
  }

 private:
  std::map<std::string, std::string> finalized_trials_;
};

class PlatformExperienceFeaturesTest : public testing::Test {
 public:
  PlatformExperienceFeaturesTest() = default;
  ~PlatformExperienceFeaturesTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kShouldUseSpecificPEHNotificationText, {}},
         {kDisablePEHNotifications, {}}},
        {});
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    base::PathService::Override(chrome::DIR_USER_DATA,
                                user_data_dir_.GetPath());
  }

  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  void CreateSentinelFile() {
    base::FilePath sentinel_path =
        user_data_dir()
            .Append(FILE_PATH_LITERAL("PlatformExperienceHelper"))
            .Append(FILE_PATH_LITERAL("LoadFeatures"));
    ASSERT_TRUE(base::CreateDirectory(sentinel_path.DirName()));
    ASSERT_TRUE(base::WriteFile(sentinel_path, ""));
  }

 private:
  base::ScopedTempDir user_data_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlatformExperienceFeaturesTest, ActivateExperiments_FileMissing) {
  TestFieldTrialObserver observer;

  ActivateFieldTrials();

  EXPECT_TRUE(observer.finalized_trials().empty());
}

TEST_F(PlatformExperienceFeaturesTest, ActivateExperiments_FileExists) {
  CreateSentinelFile();
  TestFieldTrialObserver observer;

  ActivateFieldTrials();

  const auto& finalized_trials = observer.finalized_trials();
  EXPECT_EQ(2u, finalized_trials.size());
}

}  // namespace platform_experience::features
