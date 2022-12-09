// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"

namespace component_updater {

namespace {

using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

}  // namespace

class ZxcvbnDataComponentInstallerPolicyTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

  ZxcvbnDataComponentInstallerPolicy& policy() { return policy_; }

  base::test::TaskEnvironment& task_env() { return task_env_; }

  const base::Version& version() const { return version_; }

  const base::Value::Dict& manifest() const { return manifest_; }

  const base::FilePath& GetPath() const {
    return component_install_dir_.GetPath();
  }

  void CreateEmptyFiles() {
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName),
        "");
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kFemaleNamesTxtFileName),
        "");
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kMaleNamesTxtFileName),
        "");
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kPasswordsTxtFileName),
        "");
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kSurnamesTxtFileName),
        "");
    base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kUsTvAndFilmTxtFileName),
        "");
  }

 private:
  base::test::TaskEnvironment task_env_;
  base::Version version_;
  base::Value::Dict manifest_;
  ZxcvbnDataComponentInstallerPolicy policy_;
  base::ScopedTempDir component_install_dir_;
};

// Tests that VerifyInstallation only returns true when all expected files are
// present.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest, VerifyInstallation) {
  // An empty dir lacks all required files.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateEmptyFiles();
  // All files should exist.
  EXPECT_TRUE(policy().VerifyInstallation(manifest(), GetPath()));

  base::DeleteFile(GetPath().Append(
      ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName));
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

// Tests that ComponentReady reads in the file contents and properly populates
// zxcvbn::default_ranked_dicts().
TEST_F(ZxcvbnDataComponentInstallerPolicyTest, ComponentReady) {
  // Empty / non-existent files should result in empty dictionaries.
  policy().ComponentReady(version(), GetPath(), base::Value::Dict());
  task_env().RunUntilIdle();

  // Populated files should be read and fed to the correct ranked zxcvbn
  // dictionary.
  base::WriteFile(
      GetPath().Append(
          ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName),
      "english\nwikipedia");
  base::WriteFile(
      GetPath().Append(
          ZxcvbnDataComponentInstallerPolicy::kFemaleNamesTxtFileName),
      "female\nfnames");
  base::WriteFile(
      GetPath().Append(
          ZxcvbnDataComponentInstallerPolicy::kMaleNamesTxtFileName),
      "male\nmnames");
  base::WriteFile(
      GetPath().Append(
          ZxcvbnDataComponentInstallerPolicy::kPasswordsTxtFileName),
      "passwords");
  base::WriteFile(GetPath().Append(
                      ZxcvbnDataComponentInstallerPolicy::kSurnamesTxtFileName),
                  "surnames");
  base::WriteFile(
      GetPath().Append(
          ZxcvbnDataComponentInstallerPolicy::kUsTvAndFilmTxtFileName),
      "us\ntv\nand\nfilm");

  policy().ComponentReady(version(), GetPath(), base::Value::Dict());
  task_env().RunUntilIdle();

  zxcvbn::RankedDicts& ranked_dicts = zxcvbn::default_ranked_dicts();
  EXPECT_EQ(ranked_dicts.Find("english"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("wikipedia"), 2UL);
  EXPECT_EQ(ranked_dicts.Find("female"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("fnames"), 2UL);
  EXPECT_EQ(ranked_dicts.Find("male"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("mnames"), 2UL);
  EXPECT_EQ(ranked_dicts.Find("passwords"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("surnames"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("us"), 1UL);
  EXPECT_EQ(ranked_dicts.Find("tv"), 2UL);
  EXPECT_EQ(ranked_dicts.Find("and"), 3UL);
  EXPECT_EQ(ranked_dicts.Find("film"), 4UL);
}

}  // namespace component_updater
