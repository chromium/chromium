// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace allocator {

using testing::AllOf;
using testing::HasSubstr;

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
TEST(PartitionAllocSupportTest, ProposeSyntheticFinchTrials_BRPAndPCScan) {
  for (bool pcscan_enabled : {false, true}) {
    test::ScopedFeatureList pcscan_scope;
    std::vector<test::FeatureRef> empty_list = {};
    std::vector<test::FeatureRef> pcscan_list = {
        features::kPartitionAllocPCScanBrowserOnly};
    pcscan_scope.InitWithFeatures(pcscan_enabled ? pcscan_list : empty_list,
                                  pcscan_enabled ? empty_list : pcscan_list);
#if !BUILDFLAG(USE_STARSCAN)
    pcscan_enabled = false;
#endif

    std::string brp_expectation;
    std::string pcscan_expectation;

    {
      test::ScopedFeatureList brp_scope;
      brp_scope.InitWithFeatures({}, {features::kPartitionAllocBackupRefPtr});

      brp_expectation = "Unavailable";
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn" : "Ignore_NoGroup";
#endif
      pcscan_expectation = "Unavailable";
#if BUILDFLAG(USE_STARSCAN)
      pcscan_expectation = pcscan_enabled ? "Enabled" : "Disabled";
#endif

      auto trials = ProposeSyntheticFinchTrials();
      auto group_iter = trials.find("BackupRefPtr_Effective");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, brp_expectation);
      group_iter = trials.find("PCScan_Effective");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, pcscan_expectation);
      group_iter = trials.find("PCScan_Effective_Fallback");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, pcscan_expectation);
    }

    {
      test::ScopedFeatureList brp_scope;
      brp_scope.InitAndEnableFeatureWithParameters(
          features::kPartitionAllocBackupRefPtr, {});

      brp_expectation = "Unavailable";
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn"
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) ||                  \
    (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX)) || \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG)
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                                       : "EnabledPrevSlot_NonRenderer";
#else
                                       : "EnabledBeforeAlloc_NonRenderer";
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#else
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                                       : "EnabledPrevSlot_BrowserOnly";
#else
                                       : "EnabledBeforeAlloc_BrowserOnly";
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) ||
        // (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX))
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      pcscan_expectation = "Unavailable";
#if BUILDFLAG(USE_STARSCAN)
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      pcscan_expectation = "Ignore_BRPIsOn";
#else
      pcscan_expectation = pcscan_enabled ? "Enabled" : "Disabled";
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#endif  // BUILDFLAG(USE_STARSCAN)

      auto trials = ProposeSyntheticFinchTrials();
      auto group_iter = trials.find("BackupRefPtr_Effective");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, brp_expectation);
      group_iter = trials.find("PCScan_Effective");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, pcscan_expectation);
      group_iter = trials.find("PCScan_Effective_Fallback");
      EXPECT_NE(group_iter, trials.end());
      EXPECT_EQ(group_iter->second, pcscan_expectation);
    }

    const std::string kEnabledMode =
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        "PrevSlot_";
#else
        "BeforeAlloc_";
#endif
    const std::vector<std::pair<std::string, std::string>> kModes = {
        {"disabled", "Disabled"},
        {"enabled", "Enabled" + kEnabledMode},
        {"disabled-but-2-way-split", "DisabledBut2WaySplit_"},
        {"disabled-but-3-way-split", "DisabledBut3WaySplit_"}};
    const std::vector<std::pair<std::string, std::string>> kProcesses = {
        {"browser-only", "BrowserOnly"},
        {"browser-and-renderer", "BrowserAndRenderer"},
        {"non-renderer", "NonRenderer"},
        {"all-processes", "AllProcesses"}};

    for (auto mode : kModes) {
      for (auto process_set : kProcesses) {
        test::ScopedFeatureList brp_scope;
        brp_scope.InitAndEnableFeatureWithParameters(
            features::kPartitionAllocBackupRefPtr,
            {{"brp-mode", mode.first},
             {"enabled-processes", process_set.first}});

        [[maybe_unused]] bool brp_truly_enabled = false;
        [[maybe_unused]] bool brp_nondefault_behavior = false;
        brp_expectation = "Unavailable";
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
        brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn" : mode.second;
        brp_truly_enabled = (mode.first == "enabled");
        brp_nondefault_behavior = (mode.first != "disabled");
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
        if (brp_expectation[brp_expectation.length() - 1] == '_') {
          brp_expectation += process_set.second;
        }
        pcscan_expectation = "Unavailable";
        std::string pcscan_expectation_fallback = "Unavailable";
#if BUILDFLAG(USE_STARSCAN)
        pcscan_expectation = brp_truly_enabled
                                 ? "Ignore_BRPIsOn"
                                 : (pcscan_enabled ? "Enabled" : "Disabled");
        pcscan_expectation_fallback =
            brp_nondefault_behavior ? "Ignore_BRPIsOn"
                                    : (pcscan_enabled ? "Enabled" : "Disabled");
#endif  // BUILDFLAG(USE_STARSCAN)

        auto trials = ProposeSyntheticFinchTrials();
        auto group_iter = trials.find("BackupRefPtr_Effective");
        EXPECT_NE(group_iter, trials.end());
        EXPECT_EQ(group_iter->second, brp_expectation);
        group_iter = trials.find("PCScan_Effective");
        EXPECT_NE(group_iter, trials.end());
        EXPECT_EQ(group_iter->second, pcscan_expectation);
        group_iter = trials.find("PCScan_Effective_Fallback");
        EXPECT_NE(group_iter, trials.end());
        EXPECT_EQ(group_iter->second, pcscan_expectation_fallback);
      }
    }
  }
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_DanglingPointerDetector) {
  std::string dpd_group =
      ProposeSyntheticFinchTrials()["DanglingPointerDetector"];

#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  EXPECT_EQ(dpd_group, "Enabled");
#else
  EXPECT_EQ(dpd_group, "Disabled");
#endif
}

// - Death tests misbehave on Android, http://crbug.com/643760.
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && !BUILDFLAG(IS_ANDROID) && \
    defined(GTEST_HAS_DEATH_TEST)

namespace {

// Install dangling raw_ptr handler and restore them when going out of scope.
class ScopedInstallDanglingRawPtrChecks {
 public:
  struct ConstructorParams {
    std::string mode = "crash";
    std::string type = "all";
  };
  ScopedInstallDanglingRawPtrChecks(ConstructorParams params) {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPartitionAllocDanglingPtr,
          {{"mode", params.mode}, {"type", params.type}}}},
        {/* disabled_features */});

    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_dereferenced_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
    InstallDanglingRawPtrChecks();
  }
  ScopedInstallDanglingRawPtrChecks()
      : ScopedInstallDanglingRawPtrChecks(ConstructorParams{}) {}
  ~ScopedInstallDanglingRawPtrChecks() {
    InstallDanglingRawPtrChecks();  // Check for leaks.
    partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetDanglingRawPtrReleasedFn(old_dereferenced_fn_);
  }

 private:
  test::ScopedFeatureList enabled_feature_list_;
  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_;
  partition_alloc::DanglingRawPtrReleasedFn* old_dereferenced_fn_;
};

}  // namespace

TEST(PartitionAllocDanglingPtrChecks, Basic) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
        partition_alloc::GetDanglingRawPtrReleasedFn()(42);
      },
      AllOf(HasSubstr("Detected dangling raw_ptr with id=0x000000000000002a:"),
            HasSubstr("[DanglingSignature]\t"),
            HasSubstr("The memory was freed at:"),
            HasSubstr("The dangling raw_ptr was released at:")));
}

// The StackTrace buffer might run out of storage and not record where the
// memory was freed. Anyway, it must still report the error.
TEST(PartitionAllocDanglingPtrChecks, FreeNotRecorded) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrReleasedFn()(42);
      },
      AllOf(HasSubstr("Detected dangling raw_ptr with id=0x000000000000002a:"),
            HasSubstr("[DanglingSignature]\tmissing\tmissing\t"),
            HasSubstr("It was not recorded where the memory was freed."),
            HasSubstr("The dangling raw_ptr was released at:")));
}

// TODO(https://crbug.com/1425095): Check for leaked refcount on Android.
#if BUILDFLAG(IS_ANDROID)
// Some raw_ptr might never release their refcount. Make sure this cause a
// crash on exit.
TEST(PartitionAllocDanglingPtrChecks, ReleaseNotRecorded) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
      },
      HasSubstr("A freed allocation is still referenced by a dangling pointer "
                "at exit, or at test end. Leaked raw_ptr/raw_ref "
                "could cause PartitionAlloc's quarantine memory bloat."
                "\n\n"
                "Memory was released on:"));
}
#endif

// Getting the same allocation reported twice in a row, without matching
// `DanglingRawPtrReleased` in between is unexpected. Make sure this kind of
// potential regression would be detected.
TEST(PartitionAllocDanglingPtrChecks, DoubleDetection) {
  EXPECT_DCHECK_DEATH_WITH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
      },
      "Check failed: !entry \\|\\| entry->id != id");
}

// Free and release from two different tasks with cross task dangling pointer
// detection enabled.
TEST(PartitionAllocDanglingPtrChecks, CrossTask) {
  BASE_EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
            .type = "cross_task",
        });

        base::test::TaskEnvironment task_environment;
        task_environment.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(partition_alloc::GetDanglingRawPtrDetectedFn(), 42));
        task_environment.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(partition_alloc::GetDanglingRawPtrReleasedFn(), 42));

        task_environment.RunUntilIdle();
      },
      AllOf(HasSubstr("Detected dangling raw_ptr with id=0x000000000000002a:"),
            HasSubstr("[DanglingSignature]\t"),
            HasSubstr("The memory was freed at:"),
            HasSubstr("The dangling raw_ptr was released at:")));
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoredFailuresClearsCache) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  base::test::TaskEnvironment task_environment;
  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(partition_alloc::GetDanglingRawPtrReleasedFn(), 42));
  task_environment.RunUntilIdle();
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoresNoTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoresSameTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  base::test::TaskEnvironment task_environment;
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        partition_alloc::GetDanglingRawPtrDetectedFn()(37);
        partition_alloc::GetDanglingRawPtrReleasedFn()(37);
      }));
  task_environment.RunUntilIdle();
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskNoFreeConsideredCrossTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureMacStackTrace) {
  const std::string stack_trace_output =
      "0   lib_1  0x0000000115fdfa12 base::F1(**) + 18\r\n"
      "1   lib_1  0x0000000115ec0043 base::F2() + 19\r\n"
      "2   lib_1  0x000000011601fb01 "
      "allocator_shim::internal::PartitionFree(foo) + 13265\r\n"
      "3   lib_1  0x0000000114831027 base::F3(bar) + 42\r\n"
      "4   lib_2  0x00000001148eae35 base::F4() + 437\r\n";
  EXPECT_EQ("base::F3(bar)",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                stack_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks, ExtractDanglingPtrSignatureMacTaskTrace) {
  const std::string task_trace_output =
      "Task trace:\r\n"
      "0   lib_1  0x00000001161fd431 base::F1() + 257\r\n"
      "1   lib_1  0x0000000115a49404 base::F2() + 68\r\n";
  EXPECT_EQ("base::F1()",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                task_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureWindowsStackTrace) {
  const std::string stack_trace_output =
      "Backtrace:\r\n"
      "\tbase::F1 [0x055643C3+19] (o:\\base\\F1.cc:329)\r\n"
      "\tallocator_shim::internal::PartitionFree [0x0648F87B+5243] "
      "(o:\\path.cc:441)\r\n"
      "\t_free_base [0x0558475D+29] (o:\\file_path.cc:142)\r\n"
      "\tbase::F2 [0x04E5B317+23] (o:\\base\\F2.cc:91)\r\n"
      "\tbase::F3 [0x04897800+544] (o:\\base\\F3.cc:638)\r\n";
  EXPECT_EQ("base::F2",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                stack_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureWindowsTaskTrace) {
  const std::string task_trace_output =
      "Task trace:\r\n"
      "Backtrace:\r\n"
      "\tbase::F1 [0x049068A3+813] (o:\\base\\F1.cc:207)\r\n"
      "\tbase::F2 [0x0490614C+192] (o:\\base\\F2.cc:116)\r\n";
  EXPECT_EQ("base::F1",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                task_trace_output));
}

#endif

TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_RendererLiveBackupRefPtr) {
  const std::string group = ProposeSyntheticFinchTrials()[std::string(
      base::features::kRendererLiveBRPSyntheticTrialName)];

#if BUILDFLAG(FORCIBLY_ENABLE_BACKUP_REF_PTR_IN_ALL_PROCESSES)
  EXPECT_EQ(group, "Enabled");
#else
  EXPECT_EQ(group, "Control");
#endif
}

#if PA_CONFIG(HAS_MEMORY_TAGGING)
TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_MemoryTaggingDogfood) {
  {
    test::ScopedFeatureList scope;
    scope.InitWithFeatures({}, {features::kPartitionAllocMemoryTagging});

    auto trials = ProposeSyntheticFinchTrials();

    auto group_iter = trials.find("MemoryTaggingDogfood");
    EXPECT_EQ(group_iter, trials.end());
  }

  {
    test::ScopedFeatureList scope;
    scope.InitWithFeatures({features::kPartitionAllocMemoryTagging}, {});

    auto trials = ProposeSyntheticFinchTrials();

    std::string expectation =
        partition_alloc::internal::base::CPU::GetInstanceNoAllocation()
                .has_mte()
            ? "Enabled"
            : "Disabled";
    auto group_iter = trials.find("MemoryTaggingDogfood");
    EXPECT_NE(group_iter, trials.end());
    EXPECT_EQ(group_iter->second, expectation);
  }
}
#endif

}  // namespace allocator
}  // namespace base
