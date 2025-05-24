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
    "0827c8b3db8a74c4c8f080814060465fdf54f0f9bc2cc913cb549b8df6f67bb3";

// Define the mapping between an AcceleratorAction and its string name.
// Example:
//   AcceleratorAction::kDevToggleUnifiedDesktop -> "DevToggleUnifiedDesktop".
constexpr static auto kAcceleratorActionToName =
    base::MakeFixedFlatMap<AcceleratorAction, std::string_view>({
#define ACCELERATOR_ACTION_ENTRY(action) \
  {AcceleratorAction::k##action, #action},
#define ACCELERATOR_ACTION_ENTRY_FIXED_VALUE(action, value) \
  {AcceleratorAction::k##action, #action},
        ACCELERATOR_ACTIONS
#undef ACCELERATOR_ACTION_ENTRY
#undef ACCELERATOR_ACTION_ENTRY_FIXED_VALUE
    });

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
  EXPECT_EQ(enums->size(), kAcceleratorActionToName.size());

  for (const auto& entry : *enums) {
    // Check that the C++ file has a definition equal to the histogram file.
    EXPECT_EQ(entry.second, kAcceleratorActionToName.find(entry.first)->second)
        << "Enum entry name: " << entry.second
        << " in enums.xml is different from enum entry name: "
        << kAcceleratorActionToName.find(entry.first)->second << " in C++ file";
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
  ASSERT_EQ(kAcceleratorActionToName.size(), kAcceleratorActionsTotalNum);
  const size_t kAcceleratorsToHashNum =
      kAcceleratorActionsTotalNum - kDebugAcceleratorActionsNum;
  const std::string hash = ash::StableHashOfCollection(
      base::span(kAcceleratorActionToName).first<kAcceleratorsToHashNum>(),
      [](const auto& item) { return item.second; });

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
