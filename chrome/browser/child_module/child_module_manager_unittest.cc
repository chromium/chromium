// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_manager.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths.h"
#else
#include "chrome/common/chrome_paths.h"
#endif

// TODO(crbug.com/558598893): Dynamic child module staging and FilePathWatcher
// are not yet supported on macOS.
#if !BUILDFLAG(IS_MAC)

namespace {
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
}  // namespace

namespace child_module {

class ChildModuleManagerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(base::CreateDirectory(GetModulesDir())); }

  // Creates a child module directory for `version` under `GetModulesDir()`.
  // If `with_manifest` is true, writes an empty sentinel manifest file.
  base::FilePath StageVersion(const base::Version& version,
                              bool with_manifest = true) {
    base::FilePath dir = GetModulesDir().AppendASCII(version.GetString());
    EXPECT_TRUE(base::CreateDirectory(dir));
    if (with_manifest) {
      EXPECT_TRUE(base::WriteFile(GetManifestPath(dir), ""));
    }
    return dir;
  }

  // Instantiates `manager_` in-place and waits until its initial background
  // directory scan has completed.
  void CreateManager() {
    manager_.emplace();
    manager_->WaitForInitialScanForTesting();
  }

  base::ScopedPathOverride path_override_{
#if BUILDFLAG(IS_WIN)
      base::DIR_EXE
#else
      chrome::DIR_USER_DATA
#endif
  };
  base::test::TaskEnvironment task_environment_;
  std::optional<ChildModuleManager> manager_;
};

TEST_F(ChildModuleManagerTest, NoVersionsYieldsNullopt) {
  CreateManager();

  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersions(), IsEmpty());
  EXPECT_EQ(manager_->GetRendererBinaryPath(base::Version("1.0.0.0")),
            base::FilePath());
}

TEST_F(ChildModuleManagerTest, DetectsPreStagedVersionOnStartup) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();

  EXPECT_THAT(manager_->GetLatestVersion(), Optional(kVersion));
  base::FilePath renderer_path = manager_->GetRendererBinaryPath(kVersion);
  EXPECT_EQ(renderer_path, GetRendererBinaryPath(kVersion));
  EXPECT_TRUE(GetModulesDir().IsParent(renderer_path));
  EXPECT_EQ(manager_->GetRendererBinaryPath(base::Version("999.0.0.0")),
            base::FilePath());
}

TEST_F(ChildModuleManagerTest, SelectsHighestVersion) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion51("147.0.7727.51");
  const base::Version kVersion52("147.0.7727.52");

  StageVersion(kVersion50);
  StageVersion(kVersion52);
  StageVersion(kVersion51);

  CreateManager();

  EXPECT_THAT(manager_->GetLatestVersion(), Optional(kVersion52));
  EXPECT_THAT(manager_->GetAvailableVersions(),
              ElementsAre(kVersion52, kVersion51, kVersion50));
}

TEST_F(ChildModuleManagerTest, IgnoresInvalidDirectories) {
  // Not a valid base::Version string.
  base::FilePath invalid_dir = GetModulesDir().AppendASCII("not-a-version");
  ASSERT_TRUE(base::CreateDirectory(invalid_dir));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(invalid_dir), ""));

  // Leading zero component (e.g. 01) fails canonical string check.
  base::FilePath leading_zero_dir = GetModulesDir().AppendASCII("01.0.0.1");
  ASSERT_TRUE(base::CreateDirectory(leading_zero_dir));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(leading_zero_dir), ""));

  // Missing manifest.
  StageVersion(base::Version("147.0.7727.60"), /*with_manifest=*/false);

  // Regular file instead of directory.
  base::WriteFile(GetModulesDir().AppendASCII("147.0.7727.61"), "content");

  CreateManager();

  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersions(), IsEmpty());
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyAddedVersion) {
  const base::Version kVersion("147.0.7727.55");
  CreateManager();
  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);

  StageVersion(kVersion);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetLatestVersion() == kVersion; }));
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyRemovedVersion) {
  const base::Version kVersion("147.0.7727.55");
  base::FilePath dir = StageVersion(kVersion);
  CreateManager();
  EXPECT_THAT(manager_->GetLatestVersion(), Optional(kVersion));

  ASSERT_TRUE(base::DeletePathRecursively(dir));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetLatestVersion() == std::nullopt; }));
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyRemovedVersionWithFallback) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion55("147.0.7727.55");

  StageVersion(kVersion50);
  base::FilePath dir55 = StageVersion(kVersion55);
  CreateManager();
  EXPECT_THAT(manager_->GetLatestVersion(), Optional(kVersion55));

  ASSERT_TRUE(base::DeletePathRecursively(dir55));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetLatestVersion() == kVersion50; }));
  EXPECT_THAT(manager_->GetAvailableVersions(), ElementsAre(kVersion50));
}

TEST_F(ChildModuleManagerTest, DetectsVersionWhenManifestAppearsLater) {
  const base::Version kVersion("147.0.7727.55");
  CreateManager();
  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);

  // Create version directory without the manifest sentinel.
  base::FilePath dir = StageVersion(kVersion, /*with_manifest=*/false);
  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);

  // Writing the manifest later must be caught by the recursive watcher and
  // trigger a new scan.
  EXPECT_TRUE(base::WriteFile(GetManifestPath(dir), ""));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetLatestVersion() == kVersion; }));
}

TEST_F(ChildModuleManagerTest, IgnoresManifestWhenItIsADirectory) {
  const base::Version kVersion("147.0.7727.55");
  // Create a directory named "manifest" instead of a file.
  base::FilePath dir = StageVersion(kVersion, /*with_manifest=*/false);
  ASSERT_TRUE(base::CreateDirectory(GetManifestPath(dir)));

  CreateManager();
  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersions(), IsEmpty());
}

#if BUILDFLAG(IS_POSIX)
TEST_F(ChildModuleManagerTest, IgnoresSymbolicLinks) {
  base::ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath target = external_dir.GetPath().AppendASCII("147.0.7727.90");
  ASSERT_TRUE(base::CreateDirectory(target));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(target), ""));

  base::FilePath symlink = GetModulesDir().AppendASCII("147.0.7727.90");
  ASSERT_TRUE(base::CreateSymbolicLink(target, symlink));

  CreateManager();
  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersions(), IsEmpty());
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(ChildModuleManagerTest, NonExistentDirectoryGraceful) {
  ASSERT_TRUE(base::DeletePathRecursively(GetModulesDir()));

  CreateManager();

  EXPECT_EQ(manager_->GetLatestVersion(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersions(), IsEmpty());
}

}  // namespace child_module

#endif  // !BUILDFLAG(IS_MAC)
