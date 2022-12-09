// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kAshDataFilePath[] = "AshData";
constexpr char kAshDataContent[] = "Hello, Ash, my old friend!";
constexpr size_t kAshDataSize = sizeof(kAshDataContent);

constexpr char kLacrosDataFilePath[] = "LacrosData";
constexpr char kLacrosDataContent[] = "Au revoir, Lacros!";
constexpr size_t kLacrosDataSize = sizeof(kLacrosDataContent);

// ID of an extension that only exists in Lacros after forward migration.
// NOTE: we use a sequence of characters that can't be an actual AppId here,
// so we can be sure that it won't be included in `kExtensionsAshOnly`.
constexpr char kLacrosOnlyExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

enum class FilesSetup {
  kAshOnly = 0,
  kLacrosOnly = 1,
  kBothChromes = 2,
  kMaxValue = kBothChromes,
};

void CreateDirectoryAndFile(const base::FilePath& directory_path,
                            const char* file_path,
                            const char* file_content,
                            int file_size) {
  ASSERT_TRUE(base::CreateDirectory(directory_path));
  ASSERT_EQ(base::WriteFile(directory_path.Append(file_path), file_content,
                            file_size),
            file_size);
}

void SetUpExtensions(const base::FilePath& ash_profile_dir,
                     const base::FilePath& lacros_profile_dir) {
  // The extension test data should have the following structure:
  // |- user
  //   |- Extensions
  //       |- <ash-only-ext>
  //           |- AshData
  //       |- <shared-ext>
  //           |- AshData
  //   |- lacros
  //       |- Default
  //           |- Extensions
  //               |- <lacros-only-ext>
  //                   |- LacrosData
  //               |- <shared-ext>
  //                   |- LacrosData
  base::FilePath ash_extensions_path =
      ash_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath);

  base::FilePath lacros_extensions_path = lacros_profile_dir.Append(
      browser_data_migrator_util::kExtensionsFilePath);

  // Generate data for a Lacros-only extension.
  CreateDirectoryAndFile(lacros_extensions_path.Append(kLacrosOnlyExtensionId),
                         kLacrosDataFilePath, kLacrosDataContent,
                         kLacrosDataSize);

  // Generate data for an Ash-only extension.
  std::string ash_only_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  CreateDirectoryAndFile(ash_extensions_path.Append(ash_only_extension_id),
                         kAshDataFilePath, kAshDataContent, kAshDataSize);

  // Generate data for an extension existing in both Chromes.
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  CreateDirectoryAndFile(ash_extensions_path.Append(both_extension_id),
                         kAshDataFilePath, kAshDataContent, kAshDataSize);
  CreateDirectoryAndFile(lacros_extensions_path.Append(both_extension_id),
                         kLacrosDataFilePath, kLacrosDataContent,
                         kLacrosDataSize);
}

void SetUpIndexedDB(const base::FilePath& ash_profile_dir,
                    const base::FilePath& lacros_profile_dir,
                    FilesSetup setup) {
  // The IndexedDB test data should have the following structure for full setup:
  // |- user
  //     |- IndexedDB
  //         |- chrome_extension_<shared-ext>_0.indexeddb.blob
  //             |- AshData
  //         |- chrome_extension_<shared-ext>_0.indexeddb.leveldb
  //             |- AshData
  //         |- chrome_extension_<ash-only-ext>_0.indexeddb.blob
  //             |- AshData
  //         |- chrome_extension_<ash-only-ext>_0.indexeddb.leveldb
  //             |- AshData
  //     |- lacros
  //         |- Default
  //             |- IndexedDB
  //                 |- chrome_extension_<shared-ext>_0.indexeddb.blob
  //                     |- LacrosData
  //                 |- chrome_extension_<shared-ext>_0.indexeddb.leveldb
  //                     |- LacrosData
  //                 |- chrome_extension_<lacros-only-ext>_0.indexeddb.blob
  //                     |- LacrosData
  //                 |- chrome_extension_<lacros-only-ext>_0.indexeddb.leveldb
  //                     |- LacrosData

  // Create IndexedDB files for the Lacros-only extension.
  if (setup != FilesSetup::kAshOnly) {
    const auto& [lacros_only_blob_path, lacros_only_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(lacros_profile_dir,
                                                      kLacrosOnlyExtensionId);
    CreateDirectoryAndFile(lacros_only_blob_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
    CreateDirectoryAndFile(lacros_only_leveldb_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
  }

  // Create IndexedDB files for the Ash-only extension.
  if (setup != FilesSetup::kLacrosOnly) {
    const char* ash_only_extension_id =
        browser_data_migrator_util::kExtensionsAshOnly[0];

    const auto& [ash_only_blob_path, ash_only_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir,
                                                      ash_only_extension_id);
    CreateDirectoryAndFile(ash_only_blob_path, kAshDataFilePath,
                           kAshDataContent, kAshDataSize);
    CreateDirectoryAndFile(ash_only_leveldb_path, kAshDataFilePath,
                           kAshDataContent, kAshDataSize);
  }

  // Create IndexedDB files for the extension existing in both Chromes.
  const char* both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];

  if (setup != FilesSetup::kAshOnly) {
    const auto& [lacros_blob_path, lacros_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(lacros_profile_dir,
                                                      both_extension_id);

    CreateDirectoryAndFile(lacros_blob_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
    CreateDirectoryAndFile(lacros_leveldb_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
  }

  if (setup != FilesSetup::kLacrosOnly) {
    const auto& [ash_blob_path, ash_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir,
                                                      both_extension_id);
    CreateDirectoryAndFile(ash_blob_path, kAshDataFilePath, kAshDataContent,
                           kAshDataSize);
    CreateDirectoryAndFile(ash_leveldb_path, kAshDataFilePath, kAshDataContent,
                           kAshDataSize);
  }
}

class BrowserDataBackMigratorTest : public testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // This corresponds to the directory structure under /home/chronos/user.
    // ./                             /* user_data_dir_ */
    // |- user/                       /* ash_profile_dir_ */
    //     |- back_migrator_tmp/      /* tmp_profile_dir_ */
    //     |- lacros/
    //         |- Default/            /* lacros_profile_dir_ */
    //             |- Extensions
    //             |- IndexedDB
    //             |- Storage
    //                 |- ext
    //     |- Cache
    //     |- Cookies
    //     |- Bookmarks
    //     |- Downloads/data

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    ash_profile_dir_ = user_data_dir_.GetPath().Append("user");

    lacros_profile_dir_ =
        ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir)
            .Append(browser_data_migrator_util::kLacrosProfilePath);

    tmp_profile_dir_ =
        ash_profile_dir_.Append(browser_data_back_migrator::kTmpDir);
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::ScopedTempDir user_data_dir_;
  base::FilePath ash_profile_dir_;
  base::FilePath lacros_profile_dir_;
  base::FilePath tmp_profile_dir_;
};

class BrowserDataBackMigratorFilesSetupTest
    : public BrowserDataBackMigratorTest,
      public testing::WithParamInterface<FilesSetup> {};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         BrowserDataBackMigratorFilesSetupTest,
                         testing::Values(FilesSetup::kAshOnly,
                                         FilesSetup::kLacrosOnly,
                                         FilesSetup::kBothChromes));

}  // namespace

TEST_F(BrowserDataBackMigratorTest, PreMigrationCleanUp) {
  // Create the temporary directory to make sure it is deleted during cleanup.
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir_));

  BrowserDataBackMigrator::TaskResult result =
      BrowserDataBackMigrator::PreMigrationCleanUp(ash_profile_dir_,
                                                   lacros_profile_dir_);
  ASSERT_EQ(result.status, BrowserDataBackMigrator::TaskStatus::kSucceeded);

  ASSERT_FALSE(base::PathExists(tmp_profile_dir_));
}

TEST_F(BrowserDataBackMigratorTest, MergeCommonExtensionsDataFiles) {
  SetUpExtensions(ash_profile_dir_, lacros_profile_dir_);

  ASSERT_TRUE(BrowserDataBackMigrator::MergeCommonExtensionsDataFiles(
      ash_profile_dir_, lacros_profile_dir_, tmp_profile_dir_,
      browser_data_migrator_util::kExtensionsFilePath));

  // Expected structure after this merge step:
  // |- user
  //   |- Extensions
  //       |- <ash-only-ext>
  //           |- AshData
  //       |- <shared-ext>
  //           |- AshData
  //   |- back_migrator_tmp
  //       |- Extensions
  //           |- <shared-ext>
  //               |- LacrosData
  //   |- lacros
  //       |- Default
  //           |- Extensions
  //               |- <lacros-only-ext>
  //                   |- LacrosData
  //               |- <shared-ext>
  //                   |- LacrosData
  base::FilePath tmp_extensions_path =
      tmp_profile_dir_.Append(browser_data_migrator_util::kExtensionsFilePath);

  // The Lacros-only extension data does not exist at this point.
  ASSERT_FALSE(
      base::PathExists(tmp_extensions_path.Append(kLacrosOnlyExtensionId)
                           .Append(kLacrosDataFilePath)));

  // The Ash-only extension data does not exist.
  std::string ash_only_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  ASSERT_FALSE(
      base::PathExists(tmp_extensions_path.Append(ash_only_extension_id)
                           .Append(kAshDataFilePath)));

  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  // The Ash version of the both-Chromes extension does not exist.
  ASSERT_FALSE(base::PathExists(
      tmp_extensions_path.Append(both_extension_id).Append(kAshDataFilePath)));

  // The Lacros version of the both-Chromes extension exists.
  base::FilePath lacros_tmp_file_path =
      tmp_extensions_path.Append(both_extension_id).Append(kLacrosDataFilePath);
  ASSERT_TRUE(base::PathExists(lacros_tmp_file_path));

  // The contents of the file in the temporary directory are the same as the
  // contents of the file in the original Lacros directory.
  base::FilePath lacros_original_file_path =
      lacros_profile_dir_
          .Append(browser_data_migrator_util::kExtensionsFilePath)
          .Append(kLacrosOnlyExtensionId)
          .Append(kLacrosDataFilePath);

  std::string tmp_data;
  ASSERT_TRUE(base::ReadFileToString(lacros_tmp_file_path, &tmp_data));
  std::string original_data;
  ASSERT_TRUE(
      base::ReadFileToString(lacros_original_file_path, &original_data));
  EXPECT_EQ(tmp_data, original_data);
}

TEST_P(BrowserDataBackMigratorFilesSetupTest, MergeCommonIndexedDB) {
  auto files_setup = GetParam();
  SetUpIndexedDB(ash_profile_dir_, lacros_profile_dir_, files_setup);

  const char* extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];

  ASSERT_TRUE(BrowserDataBackMigrator::MergeCommonIndexedDB(
      ash_profile_dir_, lacros_profile_dir_, extension_id));

  const auto& [ash_blob_path, ash_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir_,
                                                    extension_id);
  const auto& [lacros_blob_path, lacros_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(lacros_profile_dir_,
                                                    extension_id);

  // The Lacros files do not exist - they've either been moved to Ash or they
  // did not exist in the first place.
  ASSERT_FALSE(base::PathExists(lacros_blob_path.Append(kLacrosDataFilePath)));
  ASSERT_FALSE(
      base::PathExists(lacros_leveldb_path.Append(kLacrosDataFilePath)));

  if (files_setup == FilesSetup::kAshOnly) {
    // The Ash version is still in Ash.
    ASSERT_TRUE(base::PathExists(ash_blob_path.Append(kAshDataFilePath)));
    ASSERT_TRUE(base::PathExists(ash_leveldb_path.Append(kAshDataFilePath)));
  } else {
    // The Ash version has been deleted.
    ASSERT_FALSE(base::PathExists(ash_blob_path.Append(kAshDataFilePath)));
    ASSERT_FALSE(base::PathExists(ash_leveldb_path.Append(kAshDataFilePath)));

    // The Lacros version has been moved to Ash.
    ASSERT_TRUE(base::PathExists(ash_blob_path.Append(kLacrosDataFilePath)));
    ASSERT_TRUE(base::PathExists(ash_leveldb_path.Append(kLacrosDataFilePath)));
  }
}

namespace {

// This implementation of RAII for LacrosDataBackwardMigrationMode is intended
// to make it easy reset the state between runs.
class ScopedLacrosDataBackwardMigrationModeCache {
 public:
  explicit ScopedLacrosDataBackwardMigrationModeCache(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    SetLacrosDataBackwardMigrationMode(mode);
  }
  ScopedLacrosDataBackwardMigrationModeCache(
      const ScopedLacrosDataBackwardMigrationModeCache&) = delete;
  ScopedLacrosDataBackwardMigrationModeCache& operator=(
      const ScopedLacrosDataBackwardMigrationModeCache&) = delete;
  ~ScopedLacrosDataBackwardMigrationModeCache() {
    crosapi::browser_util::ClearLacrosDataBackwardMigrationModeCacheForTest();
  }

 private:
  void SetLacrosDataBackwardMigrationMode(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosDataBackwardMigrationMode,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosDataBackwardMigrationModeName(mode)),
               /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosDataBackwardMigrationMode(policy);
  }
};

// This implementation of RAII for the backward migration flag to make it easy
// to reset state between tests.
class ScopedLacrosDataBackwardMigrationModeCommandLine {
 public:
  explicit ScopedLacrosDataBackwardMigrationModeCommandLine(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    cmdline->AppendSwitchASCII(
        crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
        GetLacrosDataBackwardMigrationModeName(mode));
  }
  ScopedLacrosDataBackwardMigrationModeCommandLine(
      const ScopedLacrosDataBackwardMigrationModeCommandLine&) = delete;
  ScopedLacrosDataBackwardMigrationModeCommandLine& operator=(
      const ScopedLacrosDataBackwardMigrationModeCommandLine&) = delete;
  ~ScopedLacrosDataBackwardMigrationModeCommandLine() {
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    cmdline->RemoveSwitch(
        crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch);
  }
};

}  // namespace

class BrowserDataBackMigratorTriggeringTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_disabled_feature.InitAndDisableFeature(
        ash::features::kLacrosProfileBackwardMigration);
  }

 private:
  base::test::ScopedFeatureList scoped_disabled_feature;
};

TEST_F(BrowserDataBackMigratorTriggeringTest, DefaultDisabledBeforeInit) {
  EXPECT_FALSE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, DefaultDisabledAfterInit) {
  EXPECT_FALSE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, FeatureEnabledBeforeInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ash::features::kLacrosProfileBackwardMigration);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, FeatureEnabledAfterInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ash::features::kLacrosProfileBackwardMigration);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, PolicyEnabledBeforeInit) {
  // Simulate the flag being set by session_manager.
  ScopedLacrosDataBackwardMigrationModeCommandLine scoped_cmdline(
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, PolicyEnabledAfterInit) {
  ScopedLacrosDataBackwardMigrationModeCache scoped_policy(
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

}  // namespace ash
