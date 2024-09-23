// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader_factory.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::standalone_browser::BrowserSupport;

namespace crosapi {
namespace {

// Call the registered callback when Unload is started.
class UnloadObserver {
 public:
  UnloadObserver() = default;
  UnloadObserver(const UnloadObserver&) = delete;
  UnloadObserver& operator=(const UnloadObserver&) = delete;
  ~UnloadObserver() = default;

  void OnUnloadStarted() {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  void SetCallback(base::OnceClosure cb) { callback_ = std::move(cb); }

 private:
  base::OnceClosure callback_;
};

// This fake class is used to test BrowserLoader who is responsible for deciding
// which lacros selection to use.
// This class does not load nor get actual version. Such features are tested in
// RootfsLacrosLoaderTest for rootfs and StatefulLacrosLoaderTest for stateful.
class FakeLacrosSelectionLoader : public LacrosSelectionLoader {
 public:
  FakeLacrosSelectionLoader(
      const base::Version& version,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : version_(version), task_runner_(task_runner) {
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
    // Load should NOT be called when it's unloaded or busy.
    CHECK(!is_unloading_ && !is_unloaded_);

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeLacrosSelectionLoader::OnLoadCompleted,
                                  base::Unretained(this), std::move(callback)));
  }

  void Unload(base::OnceClosure callback) override {
    is_unloading_ = true;
    unload_observer_.OnUnloadStarted();

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeLacrosSelectionLoader::OnUnloadCompleted,
                                  base::Unretained(this), std::move(callback)));
  }

  bool IsUnloading() const override { return is_unloading_; }

  bool IsUnloaded() const override { return is_unloaded_; }

  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeLacrosSelectionLoader::OnGetVersionCompleted,
                       base::Unretained(this), std::move(callback)));
  }

  void SetCallbackOnUnload(base::OnceClosure cb) {
    unload_observer_.SetCallback(std::move(cb));
  }

 private:
  void OnLoadCompleted(LoadCompletionCallback callback) {
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

  void OnUnloadCompleted(base::OnceClosure callback) {
    is_unloading_ = false;
    is_unloaded_ = true;
    if (callback) {
      std::move(callback).Run();
    }
  }

  void OnGetVersionCompleted(
      base::OnceCallback<void(const base::Version&)> callback) {
    std::move(callback).Run(version_);
  }

  const base::Version version_;
  base::ScopedTempDir temp_dir_;
  base::FilePath chrome_path_;
  // `task_runner_` to run Load/Unload/GetVersion task as asynchronous
  // operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  UnloadObserver unload_observer_;

  bool is_unloaded_ = false;
  bool is_unloading_ = false;
};

class FakeLacrosSelectionLoaderFactory : public LacrosSelectionLoaderFactory {
 public:
  FakeLacrosSelectionLoaderFactory(
      const base::Version& rootfs_version,
      const base::Version& stateful_version,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : rootfs_version_(rootfs_version),
        stateful_version_(stateful_version),
        task_runner_(task_runner) {}

  FakeLacrosSelectionLoaderFactory(const FakeLacrosSelectionLoaderFactory&) =
      delete;
  FakeLacrosSelectionLoaderFactory& operator=(
      const FakeLacrosSelectionLoaderFactory&) = delete;

  ~FakeLacrosSelectionLoaderFactory() override = default;

  std::unique_ptr<LacrosSelectionLoader> CreateRootfsLacrosLoader() override {
    return std::make_unique<FakeLacrosSelectionLoader>(rootfs_version_,
                                                       task_runner_);
  }

  std::unique_ptr<LacrosSelectionLoader> CreateStatefulLacrosLoader() override {
    return std::make_unique<FakeLacrosSelectionLoader>(stateful_version_,
                                                       task_runner_);
  }

 private:
  // These versions will be set on initializing lacros selection loaders.
  const base::Version rootfs_version_ = base::Version();
  const base::Version stateful_version_ = base::Version();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// This implementation of RAII for LacrosSelection is to make it easy reset
// the state between runs.
class ScopedLacrosSelectionCache {
 public:
  explicit ScopedLacrosSelectionCache(
      ash::standalone_browser::LacrosSelectionPolicy lacros_selection) {
    SetLacrosSelection(lacros_selection);
  }
  ScopedLacrosSelectionCache(const ScopedLacrosSelectionCache&) = delete;
  ScopedLacrosSelectionCache& operator=(const ScopedLacrosSelectionCache&) =
      delete;
  ~ScopedLacrosSelectionCache() {
    ash::standalone_browser::ClearLacrosSelectionCacheForTest();
  }

 private:
  void SetLacrosSelection(
      ash::standalone_browser::LacrosSelectionPolicy lacros_selection) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosSelection, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosSelectionPolicyName(lacros_selection)),
               /*external_data_fetcher=*/nullptr);
    ash::standalone_browser::CacheLacrosSelection(policy);
  }
};

}  // namespace

class BrowserLoaderTest : public testing::Test {
 public:
  BrowserLoaderTest() {
    EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());
  }

 protected:
  BrowserLoader CreateBrowserLoaderWithFakeSelectionLoaders(
      const base::Version& rootfs_lacros_version,
      const base::Version& stateful_lacros_version) {
    return BrowserLoader(std::make_unique<FakeLacrosSelectionLoaderFactory>(
        rootfs_lacros_version, stateful_lacros_version,
        task_environment_.GetMainThreadTaskRunner()));
  }

  void WaitForTaskComplete() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionNeitherIsAvailable) {
  // If both stateful and rootfs lacros-chrome version is invalid, the chrome
  // path should be empty.
  const base::Version rootfs_lacros_version = base::Version();
  const base::Version stateful_lacros_version = base::Version();
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_TRUE(future.Get<0>().empty());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionStatefulIsUnavailable) {
  // Pass invalid `base::Version` to stateful lacros-chrome and set valid
  // version to rootfs lacros-chrome.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version();
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kRootfs, future.Get<1>());
  EXPECT_EQ(rootfs_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsUnavailable) {
  // Pass invalid `base::Version` as a rootfs lacros-chrome version.
  const base::Version rootfs_lacros_version = base::Version();
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsNewer) {
  // Use rootfs when a stateful lacros-chrome version is older.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kRootfs, future.Get<1>());
  EXPECT_EQ(rootfs_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsOlder) {
  // Use stateful when a rootfs lacros-chrome version is older.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionSameVersions) {
  // Use stateful when rootfs and stateful lacros-chrome versions are the same.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("2.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());
  EXPECT_EQ(LacrosSelection::kStateful, future.Get<1>());
  EXPECT_EQ(stateful_lacros_version, future.Get<2>());
}

TEST_F(BrowserLoaderTest, OnLoadSelectionPolicyIsRootfs) {
  ScopedLacrosSelectionCache cache(
      ash::standalone_browser::LacrosSelectionPolicy::kRootfs);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::standalone_browser::kLacrosSelectionSwitch,
      ash::standalone_browser::kLacrosSelectionStateful);

  // Set stateful lacros version newer than rootfs to test that the selection
  // policy is prioritized higher.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kRootfs);
  EXPECT_FALSE(BrowserLoader::WillLoadStatefulComponentBuilds());

  // Check stateful lacros loader is not initialized since the selection is
  // forced by policy.
  EXPECT_FALSE(browser_loader.stateful_lacros_loader_);
}

TEST_F(BrowserLoaderTest,
       OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsRootfs) {
  ScopedLacrosSelectionCache cache(
      ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::standalone_browser::kLacrosSelectionSwitch,
      ash::standalone_browser::kLacrosSelectionRootfs);

  // Set stateful lacros version newer than rootfs to test that the user choice
  // is prioritized higher.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kRootfs);
  EXPECT_FALSE(BrowserLoader::WillLoadStatefulComponentBuilds());

  // Check stateful lacros loader is not initialized since the selection is
  // forced by policy.
  EXPECT_FALSE(browser_loader.stateful_lacros_loader_);
}

TEST_F(BrowserLoaderTest,
       OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsStateful) {
  ScopedLacrosSelectionCache cache(
      ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::standalone_browser::kLacrosSelectionSwitch,
      ash::standalone_browser::kLacrosSelectionStateful);

  // Set rootfs lacros version newer than rootfs to test that the user choice
  // is prioritized higher.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("1.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<const base::FilePath&, LacrosSelection, base::Version>
      future;
  browser_loader.Load(future.GetCallback());

  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(selection, LacrosSelection::kStateful);
  EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());

  // Check rootfs lacros loader is not initialized since the selection is forced
  // by policy.
  EXPECT_FALSE(browser_loader.rootfs_lacros_loader_);
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
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version> future;
  browser_loader.Load(future.GetCallback<const base::FilePath&, LacrosSelection,
                                         base::Version>());

  const base::FilePath path = future.Get<0>();
  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(path, lacros_chrome_path);
  EXPECT_EQ(selection, LacrosSelection::kDeployedLocally);

  // Check both rootfs and stateful lacros loader are not initialized since the
  // selection is forced by switch.
  EXPECT_FALSE(browser_loader.rootfs_lacros_loader_);
  EXPECT_FALSE(browser_loader.stateful_lacros_loader_);
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
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version> future;
  browser_loader.Load(future.GetCallback<const base::FilePath&, LacrosSelection,
                                         base::Version>());

  const base::FilePath path = future.Get<0>();
  const LacrosSelection selection = future.Get<1>();
  EXPECT_EQ(path, lacros_chrome_dir.Append("chrome"));
  EXPECT_EQ(selection, LacrosSelection::kDeployedLocally);

  // Check both rootfs and stateful lacros loader are not initialized since the
  // selection is forced by switch.
  EXPECT_FALSE(browser_loader.rootfs_lacros_loader_);
  EXPECT_FALSE(browser_loader.stateful_lacros_loader_);
}

TEST_F(BrowserLoaderTest, LoadWhileUnloading) {
  // If stateful is newer, rootfs lacros will be unloaded.
  const base::Version rootfs_lacros_version = base::Version("2.0.0");
  const base::Version stateful_lacros_version = base::Version("3.0.0");
  auto browser_loader = CreateBrowserLoaderWithFakeSelectionLoaders(
      rootfs_lacros_version, stateful_lacros_version);

  // Load once. This will start asynchronous unload for rootfs lacros loader.
  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version> future;
  browser_loader.Load(future.GetCallback<const base::FilePath&, LacrosSelection,
                                         base::Version>());

  // Wait until rootfs lacros loader starts Unload. GetVersion runs
  // asynchronously before it start unloading.
  base::test::TestFuture<void> future1;
  FakeLacrosSelectionLoader* rootfs_lacros_loader =
      static_cast<FakeLacrosSelectionLoader*>(
          browser_loader.rootfs_lacros_loader_.get());
  rootfs_lacros_loader->SetCallbackOnUnload(future1.GetCallback());
  ASSERT_TRUE(future1.Wait());

  // On requesting Load while Unloading, the load request should be stored to
  // `callback_on_unload_completion_` and wait for unload to complete to resume
  // load request.
  base::test::TestFuture<base::FilePath, LacrosSelection, base::Version>
      future2;
  browser_loader.Load(future2.GetCallback<const base::FilePath&,
                                          LacrosSelection, base::Version>());

  EXPECT_EQ(LacrosSelection::kStateful, future2.Get<1>());
}

}  // namespace crosapi
