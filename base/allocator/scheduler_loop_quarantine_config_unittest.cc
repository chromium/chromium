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
  // Network process.
  "utility.network.mojom.NetworkService": {
    "global": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": true,
      "branch-capacity-in-bytes": 600
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
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_EQ(100, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(300, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kMain);
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

  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.network.mojom.NetworkService",
      SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_EQ(600, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "utility.net..workService/global");
}

constexpr char kWildcardMatchingConfigJSON[] = R"({
  "*": {
    "global": {
      "branch-capacity-in-bytes": 100
    },
  },
  "utility.*": {
    "*": {
      "branch-capacity-in-bytes": 200
    },
    "main": {
      "branch-capacity-in-bytes": 250
    }
  },
  "utility.network.*": {
    "global": {
      "branch-capacity-in-bytes": 300
    }
  },
  "utility.network.mojom.NetworkService": {
    "global": {
      "branch-capacity-in-bytes": 400
    },
  },
})";

TEST(SchedulerLoopQuarantineConfigTest, WildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kWildcardMatchingConfigJSON}});

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  // 1. Exact process match wins.
  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.network.mojom.NetworkService",
      SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_EQ(400, config.branch_capacity_in_bytes);

  // 2. Longest process prefix wins ("utility.network.*" > "utility.*").
  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.network.mojom.FooService",
      SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_EQ(300, config.branch_capacity_in_bytes);

  // 3. Prefix match wins over global wildcard ("utility.*" > "*").
  // This also tests branch-level wildcard fallback within the same process
  // match.
  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.other.Service", SchedulerLoopQuarantineBranchType::kIO);
  EXPECT_EQ(200, config.branch_capacity_in_bytes);

  // 4. Exact branch match within a process match.
  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.other.Service", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_EQ(250, config.branch_capacity_in_bytes);

  // 5. Fallback across process patterns.
  // "utility.network.mojom.FooService" matches "utility.network.*", but that
  // entry only has "global". So it should fall back to "utility.*" which has
  // a "main" branch.
  config = GetSchedulerLoopQuarantineConfiguration(
      "utility.network.mojom.FooService",
      SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_EQ(250, config.branch_capacity_in_bytes);

  // 6. Global wildcard fallback.
  config = GetSchedulerLoopQuarantineConfiguration(
      "renderer", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_EQ(100, config.branch_capacity_in_bytes);
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
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_FALSE(config.enable_quarantine);
  EXPECT_FALSE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(0, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kMain);
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
