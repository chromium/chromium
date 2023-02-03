// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"

namespace component_updater {

namespace {

using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

constexpr char kTextfilesOnlyVersion[] = "1";
constexpr char kMemoryMappedVersion[] = "2";
constexpr char kFutureVersion[] = "2.1";

}  // namespace

class ZxcvbnDataComponentInstallerPolicyTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());

    SetVersion(kTextfilesOnlyVersion);
  }

  ZxcvbnDataComponentInstallerPolicy& policy() { return policy_; }

  base::test::TaskEnvironment& task_env() { return task_env_; }

  const base::Version& version() const { return version_; }

  const base::Value::Dict& manifest() const { return manifest_; }

  const base::FilePath& GetPath() const {
    return component_install_dir_.GetPath();
  }

  void SetVersion(base::StringPiece version_str) {
    version_ = base::Version(version_str);
    manifest_.Set("version", version_str);
  }

  void CreateEmptyTextFiles() {
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

  void CreateEmptyCombinedBinaryFile() {
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName),
        ""));
  }

  void CreateTextFiles() {
    // Populated files should be read and fed to the correct ranked zxcvbn
    // dictionary.
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName),
        "english\nwikipedia"));
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kFemaleNamesTxtFileName),
        "female\nfnames"));
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kMaleNamesTxtFileName),
        "male\nmnames"));
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kPasswordsTxtFileName),
        "passwords"));
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kSurnamesTxtFileName),
        "surnames"));
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kUsTvAndFilmTxtFileName),
        "us\ntv\nand\nfilm"));
  }

  void CreateCombinedBinaryFile() {
    // This replicates the internal data structure of `zxcvbn::RankedDicts`.
    std::vector<uint8_t> binary_data;
    constexpr uint8_t MARKER_BIT = 0x80;
    auto add_entry = [&binary_data](uint16_t rank, base::StringPiece word) {
      ASSERT_LT(rank, 1 << 15);
      binary_data.push_back((rank >> 8) | MARKER_BIT);
      binary_data.push_back(rank & 0xff);
      for (const char letter : word) {
        binary_data.push_back(letter);
      }
    };

    // The entries must be ordered alphabetically to replicate the internal
    // structure of `RankedDicts`.
    add_entry(3UL, "and");
    add_entry(1UL, "english");
    add_entry(1UL, "female");
    add_entry(4UL, "film");
    add_entry(2UL, "fnames");
    add_entry(1UL, "male");
    add_entry(2UL, "mnames");
    add_entry(1UL, "passwords");
    add_entry(1UL, "surnames");
    add_entry(2UL, "tv");
    add_entry(1UL, "us");
    add_entry(2UL, "wikipedia");
    binary_data.push_back(MARKER_BIT);

    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName),
        std::move(binary_data)));
  }

  void VerifyRankedDicts() {
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

 private:
  base::test::TaskEnvironment task_env_;
  base::Version version_;
  base::Value::Dict manifest_;
  ZxcvbnDataComponentInstallerPolicy policy_;
  base::ScopedTempDir component_install_dir_;
};

// Tests that VerifyInstallation only returns true when all expected text files
// are present.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationForTextfileOnlyVersion) {
  // An empty directory lacks all required files.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateEmptyTextFiles();
  // All files should exist.
  EXPECT_TRUE(policy().VerifyInstallation(manifest(), GetPath()));

  base::DeleteFile(GetPath().Append(
      ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName));
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

// Tests that VerifyInstallation only returns true when both the text files and
// the combined binary file are present in the case of the version with support
// for memory mapping.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationForMemoryMappedVersion) {
  SetVersion(kMemoryMappedVersion);
  // An empty directory lacks all required files.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateEmptyTextFiles();
  // The combined data file is still missing.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateEmptyCombinedBinaryFile();
  EXPECT_TRUE(policy().VerifyInstallation(manifest(), GetPath()));

  base::DeleteFile(GetPath().Append(
      ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName));
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationExpectsValidVersion) {
  CreateEmptyTextFiles();
  EXPECT_TRUE(policy().VerifyInstallation(manifest(), GetPath()));

  // Verification fails for a missing version.
  EXPECT_FALSE(policy().VerifyInstallation(base::Value::Dict(), GetPath()));

  // Verification fails for an invalid version.
  SetVersion("1.x2");
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

TEST_F(ZxcvbnDataComponentInstallerPolicyTest, ComponentReadyForMissingFiles) {
  // Empty / non-existent files should result in empty dictionaries.
  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  EXPECT_FALSE(zxcvbn::default_ranked_dicts().Find("english").has_value());
}

// Tests that ComponentReady reads in the file contents and properly populates
// zxcvbn::default_ranked_dicts().
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyForTextfileOnlyVersion) {
  CreateTextFiles();

  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();
}

// Tests that ComponentReady reads in the file contents and properly populates
// zxcvbn::default_ranked_dicts() when using a memory mapped file.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyForMemoryMappedVersion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kMemoryMapWeaknessCheckDictionaries);
  SetVersion(kMemoryMappedVersion);
  CreateEmptyTextFiles();
  CreateCombinedBinaryFile();

  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();
}

TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyForMemoryMappedVersionWithDisabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kMemoryMapWeaknessCheckDictionaries);
  SetVersion(kMemoryMappedVersion);
  CreateTextFiles();
  CreateEmptyCombinedBinaryFile();

  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();
}

// Tests that updates are handled gracefully and despite potentially blocking
// behavior while closing a memory mapped file.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyHandlesUpdateProperly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kMemoryMapWeaknessCheckDictionaries);

  SetVersion(kMemoryMappedVersion);
  CreateEmptyTextFiles();
  CreateCombinedBinaryFile();

  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();

  SetVersion(kFutureVersion);
  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();
}

}  // namespace component_updater
