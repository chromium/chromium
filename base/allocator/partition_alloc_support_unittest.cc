// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::allocator {

using testing::AllOf;
using testing::HasSubstr;

TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_DanglingPointerDetector) {
  std::string dpd_group =
      ProposeSyntheticFinchTrials()["DanglingPointerDetector"];

#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  EXPECT_EQ(dpd_group, "Enabled");
#else
  EXPECT_EQ(dpd_group, "Disabled");
#endif
}

// - Death tests misbehave on Android, http://crbug.com/643760.
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && !BUILDFLAG(IS_ANDROID) && \
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

// TODO(crbug.com/40260713): Check for leaked refcount on Android.
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
      FROM_HERE, base::BindOnce([] {
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
      "\tbase::F1 [0x049068A3+813] (o:\\base\\F1.cc:207)\r\n"
      "\tbase::F2 [0x0490614C+192] (o:\\base\\F2.cc:116)\r\n";
  EXPECT_EQ("base::F1",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                task_trace_output));
}

#endif

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
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
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

class MemoryReclaimerSupportTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {base::features::kPartitionAllocMemoryReclaimer,
         base::allocator::kDisableMemoryReclaimerInBackground},
        {});
    MemoryReclaimerSupport::Instance().ResetForTesting();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::ScopedFeatureList feature_list_;
};

TEST_F(MemoryReclaimerSupportTest, StartSeveralTimes) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  // Only one task.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(MemoryReclaimerSupportTest, ForegroundToBackground) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  task_environment_.FastForwardBy(
      MemoryReclaimerSupport::kFirstPAPurgeOrReclaimDelay);
  // Task gets reposted.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  instance.SetForegrounded(false);
  task_environment_.FastForwardBy(MemoryReclaimerSupport::GetInterval());
  // But not once in background.
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(MemoryReclaimerSupportTest, ForegroundToBackgroundAndBack) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  task_environment_.FastForwardBy(
      MemoryReclaimerSupport::kFirstPAPurgeOrReclaimDelay);

  // Task gets reposted.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  instance.SetForegrounded(false);
  task_environment_.FastForwardBy(MemoryReclaimerSupport::GetInterval());
  // But not once in background.
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Until we go to foreground again.
  instance.SetForegrounded(true);
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
}

}  // namespace base::allocator
