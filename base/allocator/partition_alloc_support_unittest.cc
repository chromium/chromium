// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/dangling_raw_ptr_checks.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/feature_list.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
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
#if !PA_CONFIG(ALLOW_PCSCAN)
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
#if PA_CONFIG(ALLOW_PCSCAN)
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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX))
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
#if PA_CONFIG(ALLOW_PCSCAN)
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      pcscan_expectation = "Ignore_BRPIsOn";
#else
      pcscan_expectation = pcscan_enabled ? "Enabled" : "Disabled";
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#endif  // PA_CONFIG(ALLOW_PCSCAN)

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
#if PA_CONFIG(ALLOW_PCSCAN)
        pcscan_expectation = brp_truly_enabled
                                 ? "Ignore_BRPIsOn"
                                 : (pcscan_enabled ? "Enabled" : "Disabled");
        pcscan_expectation_fallback =
            brp_nondefault_behavior ? "Ignore_BRPIsOn"
                                    : (pcscan_enabled ? "Enabled" : "Disabled");
#endif  // PA_CONFIG(ALLOW_PCSCAN)

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
  ScopedInstallDanglingRawPtrChecks() {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPartitionAllocDanglingPtr, {{"mode", "crash"}}}},
        {/* disabled_features */});
    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_dereferenced_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
    InstallDanglingRawPtrChecks();
  }
  ~ScopedInstallDanglingRawPtrChecks() {
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
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  EXPECT_DEATH(
      partition_alloc::GetDanglingRawPtrReleasedFn()(42),
      AllOf(HasSubstr("Detected dangling raw_ptr with id=0x000000000000002a:"),
            HasSubstr("The memory was freed at:"),
            HasSubstr("The dangling raw_ptr was released at:")));
}

// The StackTrace buffer might run out of storage and not record where the
// memory was freed. Anyway, it must still report the error.
TEST(PartitionAllocDanglingPtrChecks, FreeNotRecorded) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
  EXPECT_DEATH(
      partition_alloc::GetDanglingRawPtrReleasedFn()(42),
      AllOf(HasSubstr("Detected dangling raw_ptr with id=0x000000000000002a:"),
            HasSubstr("It was not recorded where the memory was freed."),
            HasSubstr("The dangling raw_ptr was released at:")));
}

// DCHECK message are stripped in official build. It causes death tests with
// matchers to fail.
#if !defined(OFFICIAL_BUILD) || !defined(NDEBUG)
TEST(PartitionAllocDanglingPtrChecks, DoubleDetection) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  EXPECT_DCHECK_DEATH_WITH(partition_alloc::GetDanglingRawPtrDetectedFn()(42),
                           "Check failed: !entry \\|\\| entry->id != id");
}
#endif  // !defined(OFFICIAL_BUILD) || !defined(NDEBUG)

#endif

}  // namespace allocator
}  // namespace base
