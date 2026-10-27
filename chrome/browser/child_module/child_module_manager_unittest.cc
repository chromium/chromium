// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_module/features.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  // Creates a child module directory for `version` under `modules_dir_`.
  // If `with_manifest` is true, writes an empty sentinel manifest file.
  base::FilePath StageVersion(const base::Version& version,
                              bool with_manifest = true) {
    base::FilePath dir = modules_dir_.AppendASCII(version.GetString());
    EXPECT_TRUE(base::CreateDirectory(dir));
    if (with_manifest) {
      EXPECT_TRUE(base::WriteFile(GetManifestPath(dir), ""));
    }
    return dir;
  }

  // Instantiates `manager_` in-place and waits until its initial background
  // directory scan has completed.
  void CreateManager() {
    manager_.emplace(*g_browser_process->local_state());
    manager_->WaitForInitialScanForTesting();
  }

  // Returns the module that the manager is expected to provide for `version`.
  ChildModuleManager::Module ExpectedModule(
      const base::Version& version) const {
    return {
        .version = version,
        .renderer_binary_path = GetRendererBinaryPath(modules_dir_, version)};
  }

  // Selects and returns a module for a freshly instantiated RenderProcessHost.
  std::optional<ChildModuleManager::Module> SelectModuleForNewHost() {
    content::MockRenderProcessHost host(&profile_);
    return manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional();
  }

  // Selects a module for a freshly instantiated RenderProcessHost, advances
  // mock time by `uptime`, and simulates process termination with `status` and
  // `exit_code`.
  void SimulateProcessLaunchAndExit(base::TerminationStatus status,
                                    base::TimeDelta uptime,
                                    int exit_code = 0) {
    content::MockRenderProcessHost host(&profile_);
    manager_->SelectModuleForProcess(host.GetID());
    task_environment_.FastForwardBy(uptime);
    host.SimulateRenderProcessExit(status, exit_code);
  }

  // Stand in for the browser, which sets the directory at startup.
  std::optional<base::ScopedPathOverride> child_module_root_override_{
      chrome::DIR_CHILD_MODULE_ROOT};
  // The directory in which the manager is expected to look for versions.
  const base::FilePath modules_dir_ =
      base::PathService::CheckedGet(chrome::DIR_CHILD_MODULE_ROOT);
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::optional<ChildModuleManager> manager_;
};

TEST_F(ChildModuleManagerTest, NoVersionsYieldsNullopt) {
  CreateManager();

  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}

TEST_F(ChildModuleManagerTest, NoModulesDirYieldsNullopt) {
  child_module_root_override_.reset();

  CreateManager();

  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}

TEST_F(ChildModuleManagerTest, DetectsPreStagedVersionOnStartup) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();

  ASSERT_OK_AND_ASSIGN(ChildModuleManager::Module latest,
                       SelectModuleForNewHost());
  EXPECT_EQ(latest, ExpectedModule(kVersion));
  EXPECT_TRUE(modules_dir_.IsParent(latest.renderer_binary_path));
}

TEST_F(ChildModuleManagerTest, SelectsHighestVersion) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion51("147.0.7727.51");
  const base::Version kVersion52("147.0.7727.52");

  StageVersion(kVersion50);
  StageVersion(kVersion52);
  StageVersion(kVersion51);

  CreateManager();

  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion52)));
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion52, kVersion51, kVersion50));
}

TEST_F(ChildModuleManagerTest, IgnoresInvalidDirectories) {
  // Not a valid base::Version string.
  base::FilePath invalid_dir = modules_dir_.AppendASCII("not-a-version");
  ASSERT_TRUE(base::CreateDirectory(invalid_dir));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(invalid_dir), ""));

  // Leading zero component (e.g. 01) fails canonical string check.
  base::FilePath leading_zero_dir = modules_dir_.AppendASCII("01.0.0.1");
  ASSERT_TRUE(base::CreateDirectory(leading_zero_dir));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(leading_zero_dir), ""));

  // Missing manifest.
  StageVersion(base::Version("147.0.7727.60"), /*with_manifest=*/false);

  // Regular file instead of directory.
  base::WriteFile(modules_dir_.AppendASCII("147.0.7727.61"), "content");

  CreateManager();

  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyAddedVersion) {
  const base::Version kVersion("147.0.7727.55");
  CreateManager();
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);

  StageVersion(kVersion);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return SelectModuleForNewHost() == ExpectedModule(kVersion); }));
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyRemovedVersion) {
  const base::Version kVersion("147.0.7727.55");
  base::FilePath dir = StageVersion(kVersion);
  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  ASSERT_TRUE(base::DeletePathRecursively(dir));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return SelectModuleForNewHost() == std::nullopt; }));
}

TEST_F(ChildModuleManagerTest, DetectsDynamicallyRemovedVersionWithFallback) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion55("147.0.7727.55");

  StageVersion(kVersion50);
  base::FilePath dir55 = StageVersion(kVersion55);
  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion55)));

  ASSERT_TRUE(base::DeletePathRecursively(dir55));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SelectModuleForNewHost() == ExpectedModule(kVersion50);
  }));
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion50));
}

TEST_F(ChildModuleManagerTest, DetectsVersionWhenManifestAppearsLater) {
  const base::Version kVersion("147.0.7727.55");
  CreateManager();
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);

  // Create version directory without the manifest sentinel.
  base::FilePath dir = StageVersion(kVersion, /*with_manifest=*/false);
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);

  // Writing the manifest later must be caught by the recursive watcher and
  // trigger a new scan.
  EXPECT_TRUE(base::WriteFile(GetManifestPath(dir), ""));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return SelectModuleForNewHost() == ExpectedModule(kVersion); }));
}

TEST_F(ChildModuleManagerTest, IgnoresManifestWhenItIsADirectory) {
  const base::Version kVersion("147.0.7727.55");
  // Create a directory named "manifest" instead of a file.
  base::FilePath dir = StageVersion(kVersion, /*with_manifest=*/false);
  ASSERT_TRUE(base::CreateDirectory(GetManifestPath(dir)));

  CreateManager();
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}

#if BUILDFLAG(IS_POSIX)
TEST_F(ChildModuleManagerTest, IgnoresSymbolicLinks) {
  base::ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath target = external_dir.GetPath().AppendASCII("147.0.7727.90");
  ASSERT_TRUE(base::CreateDirectory(target));
  ASSERT_TRUE(base::WriteFile(GetManifestPath(target), ""));

  base::FilePath symlink = modules_dir_.AppendASCII("147.0.7727.90");
  ASSERT_TRUE(base::CreateSymbolicLink(target, symlink));

  CreateManager();
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(ChildModuleManagerTest, NonExistentDirectoryGraceful) {
  ASSERT_TRUE(base::DeletePathRecursively(modules_dir_));

  CreateManager();

  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(), IsEmpty());
}

TEST_F(ChildModuleManagerTest, DisabledByPolicyOnStartup) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);
  g_browser_process->local_state()->SetBoolean(prefs::kDynamicPatchingEnabled,
                                               false);

  CreateManager();

  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion));
}

TEST_F(ChildModuleManagerTest, DynamicallyToggledByPolicy) {
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_TRUE(local_state->GetBoolean(prefs::kDynamicPatchingEnabled));

  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion));

  local_state->SetBoolean(prefs::kDynamicPatchingEnabled, false);
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion));

  local_state->SetBoolean(prefs::kDynamicPatchingEnabled, true);
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
  EXPECT_THAT(manager_->GetAvailableVersionsForTesting(),
              ElementsAre(kVersion));
}

TEST_F(ChildModuleManagerTest, SelectAndGetModuleForProcess) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion55("147.0.7727.55");
  StageVersion(kVersion50);
  CreateManager();

  content::MockRenderProcessHost host(&profile_);
  EXPECT_EQ(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
            std::nullopt);

  EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));

  // Staging a newer version updates subsequent process selections, while `host`
  // retains the module selected for its launch.
  StageVersion(kVersion55);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SelectModuleForNewHost() == ExpectedModule(kVersion55);
  }));
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));
}

TEST_F(ChildModuleManagerTest, GetModuleForUnknownProcessIdReturnsNullopt) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);
  CreateManager();

  EXPECT_EQ(manager_->SelectModuleForProcess(content::ChildProcessId())
                .CopyAsOptional(),
            std::nullopt);
  EXPECT_EQ(
      manager_->GetModuleForProcess(content::ChildProcessId()).CopyAsOptional(),
      std::nullopt);
  EXPECT_EQ(manager_->GetModuleForProcess(content::ChildProcessId(999999))
                .CopyAsOptional(),
            std::nullopt);
}

// Dynamic child module execution is not supported and not planned on ChromeOS,
// as dynamic patching itself is not needed on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

TEST_F(ChildModuleManagerTest,
       ReselectingModuleForReusedProcessUpdatesAndClearsAssociation) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion55("147.0.7727.55");
  StageVersion(kVersion50);
  CreateManager();

  content::MockRenderProcessHost host(&profile_);
  EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));
  host.SimulateRenderProcessExit(base::TERMINATION_STATUS_NORMAL_TERMINATION,
                                 0);

  // When a reused RenderProcessHost launches a new renderer process after a
  // newer version is staged, SelectModuleForProcess() updates its association.
  StageVersion(kVersion55);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return manager_->GetAvailableVersionsForTesting().contains(kVersion55);
  }));
  EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion55)));
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion55)));
  host.SimulateRenderProcessExit(base::TERMINATION_STATUS_NORMAL_TERMINATION,
                                 0);

  // If dynamic patching is disabled on a subsequent launch of the same host,
  // SelectModuleForProcess() clears the previous association.
  g_browser_process->local_state()->SetBoolean(prefs::kDynamicPatchingEnabled,
                                               false);
  EXPECT_EQ(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
            std::nullopt);
  EXPECT_EQ(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
            std::nullopt);
}

TEST_F(ChildModuleManagerTest, CircuitBreakerBlocksAfterConsecutiveCrashes) {
  base::HistogramTester histogram_tester;
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion51("147.0.7727.51");
  StageVersion(kVersion50);
  StageVersion(kVersion51);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion51)));

  for (int i = 0; i < 2; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
    EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion51)));
    EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  }

  histogram_tester.ExpectTotalCount(
      "ChildProcess.ModuleCircuitBreaker.BlockedModulePatchVersion", 0);
  histogram_tester.ExpectTotalCount(
      "ChildProcess.ModuleCircuitBreaker.TriggerTerminationStatus", 0);

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(30));

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(),
              ElementsAre(kVersion51));
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion50)));
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.BlockedModulePatchVersion", 51, 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.TriggerTerminationStatus",
      base::TERMINATION_STATUS_PROCESS_CRASHED, 1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(ChildModuleManagerTest, CircuitBreakerBlocksOnIntegrityFailure) {
  base::HistogramTester histogram_tester;
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  for (int i = 0; i < 3; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_INTEGRITY_FAILURE,
                                 base::Seconds(5));
  }

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.BlockedModulePatchVersion", 51, 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.TriggerTerminationStatus",
      base::TERMINATION_STATUS_INTEGRITY_FAILURE, 1);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(ChildModuleManagerTest, CircuitBreakerResetsOnNormalTermination) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  for (int i = 0; i < 2; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_NORMAL_TERMINATION,
                               base::Seconds(5));

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
}

TEST_F(ChildModuleManagerTest,
       CircuitBreakerResetsOnSlowCrashExceedingThreshold) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  for (int i = 0; i < 2; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(35));

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
}

TEST_F(ChildModuleManagerTest,
       CircuitBreakerDoesNotResetOnPrematureNonCrashExits) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  for (int i = 0; i < 2; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }

  // Premature non-crash exits (e.g. killed, OOM) within fast crash threshold
  // must not reset the consecutive crash counter.
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
                               base::Seconds(5));
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_OOM, base::Seconds(5));

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
}

TEST_F(ChildModuleManagerTest,
       CircuitBreakerCountsAllFailureTerminationStatuses) {
  base::HistogramTester histogram_tester;
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_LAUNCH_FAILED,
                               base::Seconds(1));
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
                               base::Seconds(2));
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_WAS_KILLED,
                               base::Seconds(1),
                               content::RESULT_CODE_KILLED_BAD_MESSAGE);

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.TriggerTerminationStatus",
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.ModuleCircuitBreaker.BlockedModulePatchVersion", 51, 1);
}

TEST_F(ChildModuleManagerTest, CircuitBreakerRespectsCustomFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDynamicPatching,
      {{"circuit_breaker_max_consecutive_crashes", "2"},
       {"circuit_breaker_fast_crash_threshold", "10s"}});

  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  // 15s crash is above the 10s custom threshold (though below the 30s default),
  // so it must not be counted as a fast crash.
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(15));
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());

  // 2 fast crashes at 5s (<= 10s custom threshold) trip the breaker.
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());

  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
}

TEST_F(ChildModuleManagerTest, RecyclesRenderProcessHostWithoutStateLeak) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  content::MockRenderProcessHost host(&profile_);
  EXPECT_EQ(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
            std::nullopt);

  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
                Optional(ExpectedModule(kVersion)));
    EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
                Optional(ExpectedModule(kVersion)));
    task_environment_.FastForwardBy(base::Seconds(5));
    host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
    EXPECT_EQ(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              std::nullopt);
  }

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
}

TEST_F(ChildModuleManagerTest,
       HandlesSynchronousHostReinitBeforeRenderProcessExited) {
  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion51("147.0.7727.51");
  CreateManager();

  // Initial launch is unpatched before any version is staged.
  content::MockRenderProcessHost host(&profile_);
  EXPECT_EQ(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
            std::nullopt);

  StageVersion(kVersion50);
  StageVersion(kVersion51);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return manager_->GetAvailableVersionsForTesting().contains(kVersion50) &&
           manager_->GetAvailableVersionsForTesting().contains(kVersion51);
  }));

  // When the unpatched process exits and an earlier observer synchronously
  // re-initializes `host`, the unpatched exit must not be attributed to the
  // newly selected module.
  EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion51)));
  host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion51)));

  for (int i = 0; i < 3; ++i) {
    task_environment_.FastForwardBy(base::Seconds(5));

    // Simulate an earlier RenderProcessHostObserver calling host->Init() (and
    // thus SelectModuleForProcess()) synchronously during exit before
    // ChildModuleManager::RenderProcessExited() runs.
    if (i < 2) {
      EXPECT_THAT(
          manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
          Optional(ExpectedModule(kVersion51)));
    } else {
      g_browser_process->local_state()->SetBoolean(
          prefs::kDynamicPatchingEnabled, false);
      EXPECT_EQ(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
                std::nullopt);
    }
    host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
    if (i < 2) {
      // The newly selected module for the re-initialized host must remain
      // intact after the previous process's RenderProcessExited() completes.
      EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
                  Optional(ExpectedModule(kVersion51)));
      EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
    } else {
      EXPECT_EQ(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
                std::nullopt);
    }
  }

  g_browser_process->local_state()->SetBoolean(prefs::kDynamicPatchingEnabled,
                                               true);
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(),
              ElementsAre(kVersion51));
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion50)));
}

TEST_F(ChildModuleManagerTest,
       HandlesSynchronousHostReinitAfterRenderProcessExited) {
  class ReinitializingObserver : public content::RenderProcessHostObserver {
   public:
    explicit ReinitializingObserver(ChildModuleManager& manager)
        : manager_(manager) {}

    void RenderProcessExited(
        content::RenderProcessHost* host,
        const content::ChildProcessTerminationInfo& info) override {
      manager_->SelectModuleForProcess(host->GetID());
    }

   private:
    const raw_ref<ChildModuleManager> manager_;
  };

  const base::Version kVersion50("147.0.7727.50");
  const base::Version kVersion51("147.0.7727.51");
  StageVersion(kVersion50);
  StageVersion(kVersion51);
  CreateManager();

  content::MockRenderProcessHost host(&profile_);
  EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion51)));

  // Registered after the initial SelectModuleForProcess() so its
  // RenderProcessExited() runs after ChildModuleManager::RenderProcessExited().
  ReinitializingObserver reinit_observer(*manager_);
  host.AddObserver(&reinit_observer);

  for (int i = 0; i < 2; ++i) {
    task_environment_.FastForwardBy(base::Seconds(5));
    host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
    EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
                Optional(ExpectedModule(kVersion51)));
    EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  }

  task_environment_.FastForwardBy(base::Seconds(5));
  host.SimulateRenderProcessExit(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(),
              ElementsAre(kVersion51));
  EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
              Optional(ExpectedModule(kVersion50)));

  host.RemoveObserver(&reinit_observer);
}

TEST_F(ChildModuleManagerTest, BlocklistPersistsAcrossRestarts) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();

  for (int i = 0; i < 3; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));

  manager_.reset();
  CreateManager();

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), ElementsAre(kVersion));
  EXPECT_EQ(SelectModuleForNewHost(), std::nullopt);
}

TEST_F(ChildModuleManagerTest, BlocklistPurgedOnBaseVersionChange) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  base::ListValue versions;
  versions.Append(kVersion.GetString());
  base::DictValue dict;
  dict.Set("base_version", "146.0.0.0");
  dict.Set("versions", std::move(versions));
  g_browser_process->local_state()->SetDict(prefs::kDynamicPatchingBlocklist,
                                            std::move(dict));

  CreateManager();

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
  EXPECT_THAT(g_browser_process->local_state()->GetDict(
                  prefs::kDynamicPatchingBlocklist),
              IsEmpty());
}

TEST_F(ChildModuleManagerTest, CorruptedBlocklistWithoutVersionsKeyIsPurged) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  base::DictValue dict;
  dict.Set("base_version", chrome::kChromeVersion);
  g_browser_process->local_state()->SetDict(prefs::kDynamicPatchingBlocklist,
                                            std::move(dict));

  CreateManager();

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
  EXPECT_THAT(g_browser_process->local_state()->GetDict(
                  prefs::kDynamicPatchingBlocklist),
              IsEmpty());
}

TEST_F(ChildModuleManagerTest, CircuitBreakerDisabledByFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDynamicPatching,
      {{"circuit_breaker_max_consecutive_crashes", "0"}});

  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();

  for (int i = 0; i < 5; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }

  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
}

TEST_F(ChildModuleManagerTest,
       HandlesRenderProcessHostDestroyedWithoutPriorExit) {
  const base::Version kVersion("147.0.7727.51");
  StageVersion(kVersion);

  CreateManager();
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));

  {
    content::MockRenderProcessHost host(&profile_);
    EXPECT_THAT(manager_->SelectModuleForProcess(host.GetID()).CopyAsOptional(),
                Optional(ExpectedModule(kVersion)));
    EXPECT_THAT(manager_->GetModuleForProcess(host.GetID()).CopyAsOptional(),
                Optional(ExpectedModule(kVersion)));
    // `host` is destroyed at the end of this block without exit notification.
  }

  // Host destruction cleans up observation without affecting circuit breaker.
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
}

TEST_F(ChildModuleManagerTest,
       RemovingStagedVersionPrunesConsecutiveCrashCount) {
  const base::Version kVersion("147.0.7727.51");
  base::FilePath dir = StageVersion(kVersion);

  CreateManager();

  for (int i = 0; i < 2; ++i) {
    SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                                 base::Seconds(5));
  }
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());

  ASSERT_TRUE(base::DeletePathRecursively(dir));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager_->GetAvailableVersionsForTesting().empty(); }));

  StageVersion(kVersion);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return manager_->GetAvailableVersionsForTesting().contains(kVersion);
  }));
  SimulateProcessLaunchAndExit(base::TERMINATION_STATUS_PROCESS_CRASHED,
                               base::Seconds(5));
  EXPECT_THAT(manager_->GetBlockedVersionsForTesting(), IsEmpty());
  EXPECT_THAT(SelectModuleForNewHost(), Optional(ExpectedModule(kVersion)));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace child_module

#endif  // !BUILDFLAG(IS_MAC)
