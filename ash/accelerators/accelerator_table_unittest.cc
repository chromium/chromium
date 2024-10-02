// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <set>
#include <tuple>

#include "ash/accelerators/accelerator_table.h"
#include "base/hash/md5.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The number of non-Search-based accelerators.
constexpr int kNonSearchAcceleratorsNum = 112;
// The hash of non-Search-based accelerators. See HashAcceleratorData().
constexpr char kNonSearchAcceleratorsHash[] =
    "b6b832050cc5c1b6f5e0701f47343815";

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

std::string HashAcceleratorData(
    const std::vector<AcceleratorData>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const AcceleratorData& accelerator : accelerators) {
    base::MD5Update(&context, AcceleratorDataToString(accelerator));
  }
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<AcceleratorData, Cmp> accelerators;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kAcceleratorData[i];
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << AcceleratorDataToString(entry);
  }
  for (size_t i = 0; i < kDisableWithNewMappingAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kDisableWithNewMappingAcceleratorData[i];
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << AcceleratorDataToString(entry);
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedReservedActions) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kReservedActionsLength; ++i) {
    EXPECT_TRUE(actions.insert(kReservedActions[i]).second)
        << "Duplicated action: " << kReservedActions[i];
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtLoginOrLockScreen) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtLoginOrLockScreenLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtLoginOrLockScreen[i]).second)
        << "Duplicated action: " << kActionsAllowedAtLoginOrLockScreen[i];
  }
  for (size_t i = 0; i < kActionsAllowedAtLockScreenLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtLockScreen[i]).second)
        << "Duplicated action: " << kActionsAllowedAtLockScreen[i];
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtPowerMenu) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtPowerMenuLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtPowerMenu[i]).second)
        << "Duplicated action: " << kActionsAllowedAtPowerMenu[i];
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtModalWindow) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtModalWindowLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtModalWindow[i]).second)
        << "Duplicated action: " << kActionsAllowedAtModalWindow[i]
        << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedRepeatableActions) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kRepeatableActionsLength; ++i) {
    EXPECT_TRUE(actions.insert(kRepeatableActions[i]).second)
        << "Duplicated action: " << kRepeatableActions[i] << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDeprecatedAccelerators) {
  std::set<AcceleratorData, Cmp> deprecated_actions;
  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    // A deprecated action can never appear twice in the list.
    const AcceleratorData& entry = kDeprecatedAccelerators[i];
    EXPECT_TRUE(deprecated_actions.insert(entry).second)
        << "Duplicate deprecated accelerator: "
        << AcceleratorDataToString(entry);
  }

  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kDeprecatedAcceleratorsDataLength; ++i) {
    // There must never be any duplicated actions.
    const DeprecatedAcceleratorData& data = kDeprecatedAcceleratorsData[i];
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
  for (size_t i = 0; i < kAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kAcceleratorData[i];
    if (entry.modifiers & ui::EF_COMMAND_DOWN)
      continue;
    non_search_accelerators.emplace_back(entry);
  }
  for (size_t i = 0; i < kDisableWithNewMappingAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kDisableWithNewMappingAcceleratorData[i];
    if (entry.modifiers & ui::EF_COMMAND_DOWN)
      continue;
    non_search_accelerators.emplace_back(entry);
  }

  const int accelerators_number = non_search_accelerators.size();
  EXPECT_EQ(accelerators_number, kNonSearchAcceleratorsNum)
      << "All new accelerators should be Search-based and approved by UX.";

  std::stable_sort(non_search_accelerators.begin(),
                   non_search_accelerators.end(), Cmp());
  const std::string non_search_accelerators_hash =
      HashAcceleratorData(non_search_accelerators);

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
