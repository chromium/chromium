// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <string>
#include <utility>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace allocator {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
TEST(PartitionAllocSupportTest, ProposeSyntheticFinchTrials_BRPAndPCScan) {
  for (bool pcscan_enabled : {false, true}) {
    test::ScopedFeatureList pcscan_scope;
    std::vector<Feature> empty_list = {};
    std::vector<Feature> pcscan_list = {
        features::kPartitionAllocPCScanBrowserOnly};
    pcscan_scope.InitWithFeatures(pcscan_enabled ? pcscan_list : empty_list,
                                  pcscan_enabled ? empty_list : pcscan_list);
#if !defined(PA_ALLOW_PCSCAN)
    pcscan_enabled = false;
#endif

    std::string brp_expectation =
        pcscan_enabled ? "Ignore_PCScanIsOn" : "Ignore_NoGroup";
    std::string pcscan_expectation =
#if defined(PA_ALLOW_PCSCAN)
        pcscan_enabled ? "Enabled" : "Disabled";
#else
        "Unavailable";
#endif

    auto trials = ProposeSyntheticFinchTrials(false);
    auto group_iter = trials.find("BackupRefPtr_Effective");
    EXPECT_NE(group_iter, trials.end());
    EXPECT_EQ(group_iter->second, brp_expectation);
    group_iter = trials.find("PCScan_Effective");
    EXPECT_NE(group_iter, trials.end());
    EXPECT_EQ(group_iter->second, pcscan_expectation);
    group_iter = trials.find("PCScan_Effective_Fallback");
    EXPECT_NE(group_iter, trials.end());
    EXPECT_EQ(group_iter->second, pcscan_expectation);

    {
      test::ScopedFeatureList brp_scope;
      brp_scope.InitAndEnableFeatureWithParameters(
          features::kPartitionAllocBackupRefPtr, {});

      pcscan_expectation = "Unavailable";
#if BUILDFLAG(USE_BACKUP_REF_PTR)
      brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn"
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                                       : "EnabledPrevSlot_BrowserOnly";
#else
                                       : "EnabledBeforeAlloc_"
                                         "BrowserOnly";
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#if defined(PA_ALLOW_PCSCAN)
      pcscan_expectation = "Ignore_BRPIsOn";
#endif
#else  // BUILDFLAG(USE_BACKUP_REF_PTR)
      brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn" : "Ignore_NoGroup";
#if defined(PA_ALLOW_PCSCAN)
      pcscan_expectation = pcscan_enabled ? "Enabled" : "Disabled";
#endif
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

      trials = ProposeSyntheticFinchTrials(false);
      group_iter = trials.find("BackupRefPtr_Effective");
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

#if BUILDFLAG(USE_BACKUP_REF_PTR)
        brp_expectation = pcscan_enabled ? "Ignore_PCScanIsOn" : mode.second;
        bool brp_unavailable = false;
#else
        brp_expectation =
            pcscan_enabled ? "Ignore_PCScanIsOn" : "Ignore_NoGroup";
        bool brp_unavailable = true;
#endif
        ALLOW_UNUSED_LOCAL(brp_unavailable);
        if (brp_expectation[brp_expectation.length() - 1] == '_') {
          brp_expectation += process_set.second;
        }
        pcscan_expectation = "Unavailable";
        std::string pcscan_expectation_fallback = "Unavailable";
#if defined(PA_ALLOW_PCSCAN)
        pcscan_expectation =
            (brp_unavailable || mode.first.find("disabled") == 0)
                ? (pcscan_enabled ? "Enabled" : "Disabled")
                : "Ignore_BRPIsOn";
        pcscan_expectation_fallback =
            (brp_unavailable || mode.first == "disabled")
                ? (pcscan_enabled ? "Enabled" : "Disabled")
                : "Ignore_BRPIsOn";
#endif

        trials = ProposeSyntheticFinchTrials(false);
        group_iter = trials.find("BackupRefPtr_Effective");
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

}  // namespace allocator
}  // namespace base
