// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include <set>
#include <tuple>

#include "ash/public/cpp/accelerators.h"
#include "ash/test/ash_test_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The number of non-Search-based accelerators.
constexpr int kNonSearchAcceleratorsNum = 113;
// The hash of non-Search-based accelerators.
constexpr char kNonSearchAcceleratorsHash[] =
    "911675569c0f8f713f08027577f21eb620e16996f21f3aa172a6c805b9124ad8";

struct Cmp {
  bool operator()(const AcceleratorData& lhs,
                  const AcceleratorData& rhs) const {
    // Do not check |action|.
    return std::tie(lhs.trigger_on_press, lhs.keycode, lhs.modifiers) <
           std::tie(rhs.trigger_on_press, rhs.keycode, rhs.modifiers);
  }
};

std::string AcceleratorDataToString(const AcceleratorData& accelerator) {
  return base::StringPrintf(
      "trigger_on_press=%s keycode=%d shift=%s control=%s alt=%s search=%s",
      accelerator.trigger_on_press ? "true" : "false", accelerator.keycode,
      (accelerator.modifiers & ui::EF_SHIFT_DOWN) ? "true" : "false",
      (accelerator.modifiers & ui::EF_CONTROL_DOWN) ? "true" : "false",
      (accelerator.modifiers & ui::EF_ALT_DOWN) ? "true" : "false",
      (accelerator.modifiers & ui::EF_COMMAND_DOWN) ? "true" : "false");
}

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<AcceleratorData, Cmp> accelerators;
  for (const AcceleratorData& entry : kAcceleratorData) {
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << AcceleratorDataToString(entry);
  }
  for (const AcceleratorData& entry : kDisableWithNewMappingAcceleratorData) {
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << AcceleratorDataToString(entry);
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedReservedActions) {
  std::set<AcceleratorAction> actions;
  for (const AcceleratorAction& action : kReservedActions) {
    EXPECT_TRUE(actions.insert(action).second)
        << "Duplicated action: " << action;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtLoginOrLockScreen) {
  std::set<AcceleratorAction> actions;
  for (const AcceleratorAction& action : kActionsAllowedAtLoginOrLockScreen) {
    EXPECT_TRUE(actions.insert(action).second)
        << "Duplicated action: " << action;
  }
  for (const AcceleratorAction& action : kActionsAllowedAtLockScreen) {
    EXPECT_TRUE(actions.insert(action).second)
        << "Duplicated action: " << action;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtPowerMenu) {
  std::set<AcceleratorAction> actions;
  for (const AcceleratorAction& action : kActionsAllowedAtPowerMenu) {
    EXPECT_TRUE(actions.insert(action).second)
        << "Duplicated action: " << action;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtModalWindow) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtModalWindow.size(); ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtModalWindow[i]).second)
        << "Duplicated action: " << kActionsAllowedAtModalWindow[i]
        << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedRepeatableActions) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kRepeatableActions.size(); ++i) {
    EXPECT_TRUE(actions.insert(kRepeatableActions[i]).second)
        << "Duplicated action: " << kRepeatableActions[i] << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDeprecatedAccelerators) {
  std::set<AcceleratorData, Cmp> deprecated_actions;
  for (const AcceleratorData& entry : kDeprecatedAccelerators) {
    // A deprecated action can never appear twice in the list.
    EXPECT_TRUE(deprecated_actions.insert(entry).second)
        << "Duplicate deprecated accelerator: "
        << AcceleratorDataToString(entry);
  }

  std::set<AcceleratorAction> actions;
  for (const DeprecatedAcceleratorData& data : kDeprecatedAcceleratorsData) {
    // There must never be any duplicated actions.
    EXPECT_TRUE(actions.insert(data.action).second)
        << "Deprecated action: " << data.action;

    // The UMA histogram name must be of the format "Ash.Accelerators.*"
    std::string uma_histogram(data.uma_histogram_name);
    EXPECT_TRUE(base::StartsWith(uma_histogram, "Ash.Accelerators.",
                                 base::CompareCase::SENSITIVE));
  }
}

// All new accelerators should be Search-based and approved by UX.
TEST(AcceleratorTableTest, CheckSearchBasedAccelerators) {
  std::vector<AcceleratorData> non_search_accelerators;
  for (const AcceleratorData& entry : kAcceleratorData) {
    if (entry.modifiers & ui::EF_COMMAND_DOWN)
      continue;
    non_search_accelerators.emplace_back(entry);
  }
  for (const AcceleratorData& entry : kDisableWithNewMappingAcceleratorData) {
    if (entry.modifiers & ui::EF_COMMAND_DOWN)
      continue;
    non_search_accelerators.emplace_back(entry);
  }

  const int accelerators_number = non_search_accelerators.size();
  EXPECT_EQ(accelerators_number, kNonSearchAcceleratorsNum)
      << "All new accelerators should be Search-based and approved by UX.";

  std::stable_sort(non_search_accelerators.begin(),
                   non_search_accelerators.end(), Cmp());
  const std::string non_search_accelerators_hash = ash::StableHashOfCollection(
      non_search_accelerators, AcceleratorDataToString);

  EXPECT_EQ(non_search_accelerators_hash, kNonSearchAcceleratorsHash)
      << "New accelerators must use the Search key. Please talk to the UX "
         "team.\n"
         "If you are removing a non-Search-based accelerator, please update "
         "the date along with the following values\n"
      << "kNonSearchAcceleratorsNum=" << accelerators_number << " and "
      << "kNonSearchAcceleratorsHash=\"" << non_search_accelerators_hash
      << "\"";
}

}  // namespace ash
