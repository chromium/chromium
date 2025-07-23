// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_actions.h"

#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The total number of accelerator actions.
constexpr size_t kAcceleratorActionsTotalNum = 172;
// The toal number of debug accelerators, these will not be used for hashing.
constexpr size_t kDebugAcceleratorActionsNum = 29;
// The hash of accelerator actions. Please update this when adding a new
// accelerator action.
constexpr char kAcceleratorActionsHash[] =
    "5376c379e20a43cf689c1d23bc111305426d078d0a24343e2fd4f63787c61ebc";

struct TestParams {
  bool use_debug_shortcuts = false;
  bool use_dev_shortcuts = false;
};

class AcceleratorActionsTest
    : public AshTestBase,
      public ::testing::WithParamInterface<TestParams> {
 public:
  AcceleratorActionsTest() = default;

  AcceleratorActionsTest(const AcceleratorActionsTest&) = delete;
  AcceleratorActionsTest& operator=(const AcceleratorActionsTest&) = delete;

  ~AcceleratorActionsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    const TestParams& params = GetParam();
    if (params.use_debug_shortcuts) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kAshDebugShortcuts);
    }
    if (params.use_dev_shortcuts) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kAshDeveloperShortcuts);
    }
    AshTestBase::SetUp();
  }
};

}  // namespace

// Tests that the AcceleratorAction enum in enums.xml exactly matches the
// AcceleratorAction enum in C++ file.
TEST_P(AcceleratorActionsTest, CheckHistogramEnum) {
  const auto enums =
      base::ReadEnumFromEnumsXml("AcceleratorAction", "chromeos");
  ASSERT_TRUE(enums);
  // The number of enums in the histogram entry should be equal to the number of
  // enums in the C++ file.
  EXPECT_EQ(enums->size(), GetAcceleratorActionsForTest().size());

  for (const auto& entry : *enums) {
    // Check that the C++ file has a definition equal to the histogram file.
    AcceleratorAction action = static_cast<AcceleratorAction>(entry.first);
    const char* action_name = GetAcceleratorActionName(action);
    EXPECT_EQ(entry.second, action_name)
        << "Enum entry name: " << entry.second
        << " in enums.xml is different from enum entry name: " << action_name
        << " in C++ file";
  }
}

TEST_P(AcceleratorActionsTest, AcceleratorActionsHash) {
  const char kCommonMessage[] =
      "If you are adding a non-debug accelerator action, please add "
      "the new action to be bottom of the enums but before "
      "DEBUG accelerator actions. \n"
      "Please update the values `kAcceleratorActionsTotalNum` and "
      "`kDebugAcceleratorActionsNum` (if applicable).";

  // First check that the size of the enum is correct.
  auto all = GetAcceleratorActionsForTest();
  ASSERT_EQ(all.size(), kAcceleratorActionsTotalNum);
  const size_t kAcceleratorsToHashNum =
      kAcceleratorActionsTotalNum - kDebugAcceleratorActionsNum;
  auto names = base::span(all).first<kAcceleratorsToHashNum>();
  const std::string hash =
      ash::StableHashOfCollection(names, [](const auto& action) {
        return std::string(GetAcceleratorActionName(action));
      });

  EXPECT_EQ(hash, kAcceleratorActionsHash)
      << kCommonMessage << " Please update kAcceleratorActionsHash to: \n"
      << hash << "\n";
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AcceleratorActionsTest,
    ::testing::Values(TestParams{false, false},  // No shortcuts
                      TestParams{true, false},   // Debug shortcuts only
                      TestParams{false, true},   // Dev shortcuts only
                      TestParams{true, true}     // Both shortcuts
                      ));

}  // namespace ash
