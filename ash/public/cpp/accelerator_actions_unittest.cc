// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_actions.h"

#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/hash/md5.h"
#include "base/hash/md5_boringssl.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The total number of accelerator actions.
constexpr int kAcceleratorActionsTotalNum = 167;
// The toal number of debug accelerators, these will not be used for hashing.
constexpr int kDebugAcceleratorActionsNum = 28;
// The hash of accelerator actions. Please update this when adding a new
// accelerator action.
constexpr char kAcceleratorActionsHash[] = "19f19f0e593d97ece036a1e5a9905135";

// Define the mapping between an AcceleratorAction and its string name.
// Example:
//   AcceleratorAction::kDevToggleUnifiedDesktop -> "DevToggleUnifiedDesktop".
constexpr static auto kAcceleratorActionToName =
    base::MakeFixedFlatMap<AcceleratorAction, const char*>({
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
  const int current_actions_size = kAcceleratorActionToName.size();
  EXPECT_EQ(current_actions_size, kAcceleratorActionsTotalNum)
      << kCommonMessage;

  // Then check that the hash is correct.
  base::MD5Context context;
  base::MD5Init(&context);
  int iter_count = 0;
  for (const auto iter : kAcceleratorActionToName) {
    base::MD5Update(&context, iter.second);
    // Only hash up non-debug accelerator actions.
    if (++iter_count >= current_actions_size - kDebugAcceleratorActionsNum) {
      break;
    }
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  const std::string current_hash = MD5DigestToBase16(digest);

  EXPECT_EQ(current_hash, kAcceleratorActionsHash)
      << kCommonMessage << " Please update kAcceleratorActionsHash to: \n"
      << current_hash << "\n";
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
