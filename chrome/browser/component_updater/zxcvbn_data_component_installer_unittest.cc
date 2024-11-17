// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"

namespace component_updater {

namespace {

using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

constexpr char kTextfilesOnlyVersion[] = "1";
constexpr char kMemoryMappedVersion[] = "2";
constexpr char kFutureVersion[] = "2.1";

// Use this function to generate a new combined file from the updated
// dictionaries.
zxcvbn::RankedDicts ParseRankedDictionaries(const base::FilePath& install_dir) {
  std::vector<std::string> raw_dicts;
  for (const auto& file_name : ZxcvbnDataComponentInstallerPolicy::kFileNames) {
    base::FilePath dictionary_path = install_dir.Append(file_name);
    DVLOG(1) << "Reading Dictionary from file: " << dictionary_path;

    std::string dictionary;
    if (base::ReadFileToString(dictionary_path, &dictionary)) {
      raw_dicts.push_back(std::move(dictionary));
    } else {
      DVLOG(1) << "Failed reading from " << dictionary_path;
    }
  }

  // The contained StringPieces hold references to the strings in raw_dicts.
  std::vector<std::vector<std::string_view>> dicts;
  for (const auto& raw_dict : raw_dicts) {
    dicts.push_back(base::SplitStringPiece(
        raw_dict, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  }

  // This copies the words; after this call, the original strings can be
  // discarded.
  return zxcvbn::RankedDicts(dicts);
}

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

  void SetVersion(std::string_view version_str) {
    version_ = base::Version(version_str);
    manifest_.Set("version", version_str);
  }

  void CreateEmptyTextFiles() {
    for (const auto& filename :
         ZxcvbnDataComponentInstallerPolicy::kFileNames) {
      base::WriteFile(GetPath().Append(filename), "");
    }
  }

  void CreateInvalidCombinedBinaryFile() {
    constexpr uint8_t marker[1] = {0x70};
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName),
        marker));
  }

  void CreateValidCombinedBinaryFile() {
    constexpr uint8_t marker[1] = {0x80};
    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName),
        marker));
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
    zxcvbn::RankedDicts dicts = ParseRankedDictionaries(GetPath());

    ASSERT_TRUE(base::WriteFile(
        GetPath().Append(
            ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName),
        dicts.DataForTesting()));
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

// Tests that VerifyInstallation only returns true when both the text files and
// a combined binary file with a valid marker are present in the case of the
// version with support for memory mapping.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationForMemoryMappedVersion) {
  SetVersion(kMemoryMappedVersion);
  // An empty directory lacks all required files.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateEmptyTextFiles();
  // The combined data file is still missing.
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));

  CreateValidCombinedBinaryFile();
  EXPECT_TRUE(policy().VerifyInstallation(manifest(), GetPath()));

  base::DeleteFile(GetPath().Append(
      ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName));
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

// Tests that VerifyInstallation fails if the first bit of the memory mapped
// file is not a valid marker bit.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationForMemoryMappedVersionWithInvalidMarkerBit) {
  SetVersion(kMemoryMappedVersion);

  CreateEmptyTextFiles();
  CreateInvalidCombinedBinaryFile();
  EXPECT_FALSE(policy().VerifyInstallation(manifest(), GetPath()));
}

TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       VerifyInstallationExpectsValidVersion) {
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
// zxcvbn::default_ranked_dicts() when using a memory mapped file.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyForMemoryMappedVersion) {
  SetVersion(kMemoryMappedVersion);
  CreateTextFiles();
  CreateCombinedBinaryFile();

  policy().ComponentReady(version(), GetPath(), manifest().Clone());
  task_env().RunUntilIdle();

  VerifyRankedDicts();
}

// Tests that updates are handled gracefully and despite potentially blocking
// behavior while closing a memory mapped file.
TEST_F(ZxcvbnDataComponentInstallerPolicyTest,
       ComponentReadyHandlesUpdateProperly) {
  SetVersion(kMemoryMappedVersion);
  CreateTextFiles();
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
