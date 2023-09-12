// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::standalone_browser::BrowserSupport;

namespace crosapi {
namespace {

// This implementation of RAII for LacrosSelection is to make it easy reset
// the state between runs.
class ScopedLacrosSelectionCache {
 public:
  explicit ScopedLacrosSelectionCache(
      browser_util::LacrosSelectionPolicy lacros_selection) {
    SetLacrosSelection(lacros_selection);
  }
  ScopedLacrosSelectionCache(const ScopedLacrosSelectionCache&) = delete;
  ScopedLacrosSelectionCache& operator=(const ScopedLacrosSelectionCache&) =
      delete;
  ~ScopedLacrosSelectionCache() {
    browser_util::ClearLacrosSelectionCacheForTest();
  }

 private:
  void SetLacrosSelection(
      browser_util::LacrosSelectionPolicy lacros_selection) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosSelection, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosSelectionPolicyName(lacros_selection)),
               /*external_data_fetcher=*/nullptr);
    browser_util::CacheLacrosSelection(policy);
  }
};

}  // namespace

// This fake class is used to test BrowserLoader who is responsible for deciding
// which lacros selection to use.
// This class does not load nor get actual version. Such features are tested in
// RootfsLacrosLoaderTest for rootfs and StatefulLacrosLoaderTest for stateful.
class FakeLacrosSelectionLoader : public LacrosSelectionLoader {
 public:
  FakeLacrosSelectionLoader() {
    // Create dummy chrome binary path.
    CHECK(temp_dir_.CreateUniqueTempDir());
    chrome_path_ = temp_dir_.GetPath().Append(kLacrosChromeBinary);
    base::WriteFile(chrome_path_, "I am chrome binary.");
  }

  FakeLacrosSelectionLoader(const FakeLacrosSelectionLoader&) = delete;
  FakeLacrosSelectionLoader& operator=(const FakeLacrosSelectionLoader&) =
      delete;

  ~FakeLacrosSelectionLoader() override = default;

  void Load(LoadCompletionCallback callback, bool forced) override {
    if (!callback) {
      return;
    }

    // If version is invalid, returns empty path. Otherwise fill in with some
    // path. Whether path is empty or not is used as a condition to check
    // whether error has occurred during loading.
    const base::FilePath path =
        version_.IsValid() ? temp_dir_.GetPath() : base::FilePath();
    std::move(callback).Run(version_, path);
  }
  void Unload() override {}
  void Reset() override {}
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    std::move(callback).Run(version_);
  }

  void SetVersionForTesting(const base::Version& version) override {
    version_ = version;
  }

 private:
  base::Version version_ = base::Version();
  base::ScopedTempDir temp_dir_;
  base::FilePath chrome_path_;
};

class BrowserLoaderTest : public testing::Test {
 public:
  BrowserLoaderTest() {
    browser_loader_ = std::make_unique<BrowserLoader>(
        /*rootfs_lacros_loader=*/std::make_unique<FakeLacrosSelectionLoader>(),
        /*stateful_lacros_loader=*/std::make_unique<
            FakeLacrosSelectionLoader>());
    EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());
  }

  // Public because this is test code.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  std::unique_ptr<BrowserLoader> browser_loader_;
};

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionNeitherIsAvailable) {
  // If both stateful and rootfs lacros-chrome version is invalid, the chrome
  // path should be empty.
  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_TRUE(future.Get<0>().empty());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionStatefulIsUnavailable) {
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);
  // Pass invalid `base::Version` to stateful lacros-chrome and set valid
  // version to rootfs lacros-chrome.
  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kRootfs, future.Get<1>());
  EXPECT_EQ(rootfs_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsUnavailable) {
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);

  // Pass invalid `base::Version` as a rootfs lacros-chrome version.
  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsNewer) {
  // Use rootfs when a stateful lacros-chrome version is older.
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kRootfs, future.Get<1>());
  EXPECT_EQ(rootfs_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsOlder) {
  // Use stateful when a rootfs lacros-chrome version is older.
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionSameVersions) {
  // Use stateful when rootfs and stateful lacros-chrome versions are the same.
  const base::Version stateful_lacros_version = base::Version("2.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadSelectionPolicyIsRootfs) {
  ScopedLacrosSelectionCache cache(
      browser_util::LacrosSelectionPolicy::kRootfs);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      browser_util::kLacrosSelectionSwitch,
      browser_util::kLacrosSelectionStateful);

  // Set stateful lacros version newer than rootfs to test that the selection
  // policy is prioritized higher.
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kRootfs);
  EXPECT_FALSE(BrowserLoader::WillLoadStatefulComponentBuilds());
}

TEST_F(BrowserLoaderTest,
       OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsRootfs) {
  ScopedLacrosSelectionCache cache(
      browser_util::LacrosSelectionPolicy::kUserChoice);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      browser_util::kLacrosSelectionSwitch,
      browser_util::kLacrosSelectionRootfs);

  // Set stateful lacros version newer than rootfs to test that the user choice
  // is prioritized higher.
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kRootfs);
  EXPECT_FALSE(BrowserLoader::WillLoadStatefulComponentBuilds());
}

TEST_F(BrowserLoaderTest,
       OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsStateful) {
  ScopedLacrosSelectionCache cache(
      browser_util::LacrosSelectionPolicy::kUserChoice);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      browser_util::kLacrosSelectionSwitch,
      browser_util::kLacrosSelectionStateful);

  // Set rootfs lacros version newer than stateful to test that the user choice
  // is prioritized higher.
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader_->Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kStateful);
  EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());
}

TEST_F(BrowserLoaderTest, OnLoadLacrosBinarySpecifiedBySwitch) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const base::FilePath lacros_chrome_dir = temp_dir.GetPath();
  base::WriteFile(lacros_chrome_dir.Append("chrome"),
                  "I am lacros-chrome deployed locally.");
  const base::FilePath lacros_chrome_path =
      temp_dir.GetPath().Append("mychrome");
  base::WriteFile(lacros_chrome_path,
                  "I am a custom lacros-chrome deployed locally.");

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromePath, lacros_chrome_path.MaybeAsASCII());

  // Set stateful/rootfs lacros-chrome version to check that specified
  // lacros-chrome is prioritized higher.
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version> future;
  browser_loader_->Load(future.GetCallback<const base::FilePath&,
                                           LacrosSelection, base::Version>());

  const base::FilePath path = future.Get<0>();
  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(path, lacros_chrome_path);
  EXPECT_EQ(selection, LacrosSelection::kDeployedLocally);
}

TEST_F(BrowserLoaderTest, OnLoadLacrosDirectorySpecifiedBySwitch) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const base::FilePath lacros_chrome_dir = temp_dir.GetPath();
  base::WriteFile(lacros_chrome_dir.Append("chrome"),
                  "I am lacros-chrome deployed locally.");

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromePath, lacros_chrome_dir.MaybeAsASCII());

  // Set stateful/rootfs lacros-chrome version to check that specified
  // lacros-chrome is prioritized higher.
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  browser_loader_->stateful_lacros_loader_->SetVersionForTesting(
      stateful_lacros_version);
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  browser_loader_->rootfs_lacros_loader_->SetVersionForTesting(
      rootfs_lacros_version);

  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version> future;
  browser_loader_->Load(future.GetCallback<const base::FilePath&,
                                           LacrosSelection, base::Version>());

  const base::FilePath path = future.Get<0>();
  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(path, lacros_chrome_dir.Append("chrome"));
  EXPECT_EQ(selection, LacrosSelection::kDeployedLocally);
}

}  // namespace crosapi
