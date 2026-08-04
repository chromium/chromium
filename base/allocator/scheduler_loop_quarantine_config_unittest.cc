// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/scheduler_loop_quarantine_config.h"

#include "base/allocator/partition_alloc_features.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
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
      "enable-task-controlled-purge": true,
      "pause-in-between-tasks": true,
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
  // GPU process.
  "gpu-process": {
    "*": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 900
    },
    "viz-compositor": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 700
    },
    "compositor-gpu": {
      "enable-quarantine": true,
      "enable-zapping": true,
      "leak-on-destruction": false,
      "branch-capacity-in-bytes": 800
    },
  },
})";

class SchedulerLoopQuarantineConfigTest : public testing::Test {
 protected:
  void SetUp() override { ResetSchedulerLoopQuarantineConfigForTesting(); }
  void TearDown() override { ResetSchedulerLoopQuarantineConfigForTesting(); }
};

TEST_F(SchedulerLoopQuarantineConfigTest, ValidConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kValidTestingConfigJSON}});
  ResetSchedulerLoopQuarantineConfigForTesting();

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_FALSE(config.enable_task_controlled_purge);
  EXPECT_FALSE(config.pause_in_between_tasks);
  EXPECT_EQ(100, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/global");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_FALSE(config.enable_task_controlled_purge);
  EXPECT_FALSE(config.pause_in_between_tasks);
  EXPECT_EQ(300, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "browser/*");

  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_TRUE(config.enable_task_controlled_purge);
  EXPECT_TRUE(config.pause_in_between_tasks);
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

  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kVizCompositor);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(700, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/viz-compositor");

  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kCompositorGpu);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(800, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/compositor-gpu");

  // 1. ThreadLocalDefault in gpu-process gets its own "*" entry.
  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kThreadLocalDefault);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(900, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/*");

  // 2. Main in gpu-process falls back to gpu-process's "*" entry.
  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kMain);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(900, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/main");

  // 3. IO in gpu-process falls back to gpu-process's "*" entry.
  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kIO);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_FALSE(config.leak_on_destruction);
  EXPECT_EQ(900, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/io");

  // 4. Global in gpu-process falls back to "*" process wildcard's "global"
  // entry!
  config = GetSchedulerLoopQuarantineConfiguration(
      "gpu-process", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_TRUE(config.enable_zapping);
  EXPECT_TRUE(config.leak_on_destruction);
  EXPECT_EQ(100, config.branch_capacity_in_bytes);
  EXPECT_STREQ(config.branch_name, "gpu-process/global");
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

TEST_F(SchedulerLoopQuarantineConfigTest, WildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kWildcardMatchingConfigJSON}});
  ResetSchedulerLoopQuarantineConfigForTesting();

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

TEST_F(SchedulerLoopQuarantineConfigTest, InvalidConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kInvalidTestingConfigJSON}});
  ResetSchedulerLoopQuarantineConfigForTesting();

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

TEST_F(SchedulerLoopQuarantineConfigTest, CommandLineOverride) {
  base::test::ScopedCommandLine scoped_command_line;
  base::test::ScopedFeatureList feature_list;
  // Enable the feature, but with a config that we expect to be overridden.
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{base::features::kPartitionAllocSchedulerLoopQuarantineConfig.name,
        kValidTestingConfigJSON}});

  // Set the command line switch with a different config.
  constexpr char kCommandLineConfigJSON[] = R"({
    "browser": {
      "global": {
        "enable-quarantine": true,
        "branch-capacity-in-bytes": 999
      }
    }
  })";

  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kPartitionAllocSchedulerLoopQuarantine, kCommandLineConfigJSON);
  ResetSchedulerLoopQuarantineConfigForTesting();

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  // This should use the command line config, not the feature param config.
  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_EQ(999, config.branch_capacity_in_bytes);
}

TEST_F(SchedulerLoopQuarantineConfigTest, CommandLineOverrideFeatureDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::test::ScopedFeatureList feature_list;
  // Disable the feature.
  feature_list.InitAndDisableFeature(
      base::features::kPartitionAllocSchedulerLoopQuarantine);

  // Set the command line switch.
  constexpr char kCommandLineConfigJSON[] = R"({
    "browser": {
      "global": {
        "enable-quarantine": true,
        "branch-capacity-in-bytes": 999
      }
    }
  })";

  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kPartitionAllocSchedulerLoopQuarantine, kCommandLineConfigJSON);
  ResetSchedulerLoopQuarantineConfigForTesting();

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  // This should still use the command line config, even though the feature is
  // disabled.
  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_TRUE(config.enable_quarantine);
  EXPECT_EQ(999, config.branch_capacity_in_bytes);
}

TEST_F(SchedulerLoopQuarantineConfigTest, FeatureDisabledNoSwitch) {
  base::test::ScopedFeatureList feature_list;
  // Disable the feature.
  feature_list.InitAndDisableFeature(
      base::features::kPartitionAllocSchedulerLoopQuarantine);
  ResetSchedulerLoopQuarantineConfigForTesting();

  partition_alloc::internal::SchedulerLoopQuarantineConfig config;

  // This should return disabled config.
  config = GetSchedulerLoopQuarantineConfiguration(
      "browser", SchedulerLoopQuarantineBranchType::kGlobal);
  EXPECT_FALSE(config.enable_quarantine);
}

TEST_F(SchedulerLoopQuarantineConfigTest,
       HasSchedulerLoopQuarantineTaskControl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{"PartitionAllocSchedulerLoopQuarantineConfig", R"({
          "browser": {
            "main": {
              "enable-task-controlled-purge": true,
              "pause-in-between-tasks": true
            }
          },
          "gpu": {
            "main": {
              "enable-task-controlled-purge": true,
              "pause-in-between-tasks": false
            }
          },
          "utility": {
            "main": {
              "enable-task-controlled-purge": false,
              "pause-in-between-tasks": true
            }
          },
          "renderer": {
            "main": {
              "enable-task-controlled-purge": false,
              "pause-in-between-tasks": false
            }
          }
        })"}});
  ResetSchedulerLoopQuarantineConfigForTesting();

  // Enables both.
  EXPECT_TRUE(HasSchedulerLoopQuarantineTaskControl("browser"));
  // Enables task-controlled purge only.
  EXPECT_TRUE(HasSchedulerLoopQuarantineTaskControl("gpu"));
  // Enables pause-in-between-tasks only.
  EXPECT_TRUE(HasSchedulerLoopQuarantineTaskControl("utility"));
  // Enables neither.
  EXPECT_FALSE(HasSchedulerLoopQuarantineTaskControl("renderer"));
}

TEST_F(SchedulerLoopQuarantineConfigTest, HasMiracleObject) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPartitionAllocSchedulerLoopQuarantine,
      {{"PartitionAllocSchedulerLoopQuarantineConfig", R"({
          "browser": {
            "main": {
              "enable-quarantine": true
            }
          },
          "renderer": {
            "main": {
              "enable-quarantine": false
            }
          },
          "utility": {
            "amsc": {
              "enable-quarantine": true
            }
          },
          "gpu": {
            "amsc": {
              "enable-quarantine": true
            },
            "main": {
              "enable-quarantine": true
            }
          },
          "ppapi*": {
            "main": {
              "enable-quarantine": true
            }
          },
          "network": {
            "*": {
              "enable-quarantine": true
            }
          }
        })"}});
  ResetSchedulerLoopQuarantineConfigForTesting();

  EXPECT_TRUE(HasMiracleObject("browser"));
  EXPECT_FALSE(HasMiracleObject("renderer"));
  // AMSC branch is excluded from HasMiracleObject.
  EXPECT_FALSE(HasMiracleObject("utility"));
  // AMSC branch listed first is skipped, continuing to main branch.
  EXPECT_TRUE(HasMiracleObject("gpu"));
  // Process name wildcard matches "ppapi_process".
  EXPECT_TRUE(HasMiracleObject("ppapi_process"));
  // Branch name wildcard "*" matches.
  EXPECT_TRUE(HasMiracleObject("network"));
}

}  // namespace
}  // namespace base::allocator

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
