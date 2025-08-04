// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/scheduler_loop_quarantine_config.h"

#include "base/allocator/partition_alloc_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base::allocator {
namespace {

constexpr char kValidTestingConfigJSON[] = R"({
  // Process-wildcard.
  "*": {
    "global": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": true,
      "branch-capacity-in-bytes": 100
    },
    "main": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 200
    },
  },
  // Browser process.
  "browser": {
    "*": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 300
    },
    "main": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 400
    },
  },
  // Renderer process.
  "renderer": {
    "global": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": true,
      "branch-capacity-in-bytes": 500
    },
  },
})";

TEST(SchedulerLoopQuarantineConfigTest, ValidConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kValidTestingConfigJSON}});

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_EQ(100, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(300, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(400, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/main");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_EQ(500, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(200, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/main");
}

constexpr char kInvalidTestingConfigJSON[] = "nyan";

TEST(SchedulerLoopQuarantineConfigTest, InvalidConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kInvalidTestingConfigJSON}});

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/main");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "renderer/main");
}

}  // namespace
}  // namespace base::allocator

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
