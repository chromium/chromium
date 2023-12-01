// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_actions.h"

#include "base/containers/fixed_flat_map.h"
#include "base/hash/md5.h"
#include "base/hash/md5_boringssl.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The total number of accelerator actions.
constexpr int kAcceleratorActionsTotalNum = 156;
// The toal number of debug accelerators, these will not be used for hashing.
constexpr int kDebugAcceleratorActionsNum = 27;
// The hash of accelerator actions. Please update this when adding a new
// accelerator action.
constexpr char kAcceleratorActionsHash[] = "69ca25642d0ee9a9cf3eee1c3ac14419";

// Define the mapping between an AcceleratorAction and its string name.
// Example:
//   AcceleratorAction::kDevToggleUnifiedDesktop -> "DevToggleUnifiedDesktop".
constexpr static auto kAcceleratorActionToName =
    base::MakeFixedFlatMap<AcceleratorAction, const char*>({
#define ACCELERATOR_ACTION_ENTRY(action) \
  {AcceleratorAction::k##action, #action},
        ACCELERATOR_ACTIONS
#undef ACCELERATOR_ACTION_ENTRY
    });

class AcceleratorActionsTest : public testing::Test {
 public:
  AcceleratorActionsTest() = default;

  AcceleratorActionsTest(const AcceleratorActionsTest&) = delete;
  AcceleratorActionsTest& operator=(const AcceleratorActionsTest&) = delete;

  ~AcceleratorActionsTest() override = default;
};

}  // namespace

// Tests that the AcceleratorAction enum in enums.xml exactly matches the
// AcceleratorAction enum in C++ file.
TEST_F(AcceleratorActionsTest, CheckHistogramEnum) {
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

TEST_F(AcceleratorActionsTest, AcceleratorActionsHash) {
  const char kCommonMessage[] =
      "If you are adding a non-debug accelerator action, please ensure that "
      "you add the new action to be bottom of the enums but before the"
      "DEBUG accelerator actions. Please update the values "
      "`kAcceleratorActionsTotalNum` and `kDebugAcceleratorActionsNum` "
      "(if applicable)."
      "Please then update `kAcceleratorActionsHash`";

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
      << kCommonMessage << "kAcceleratorActionsHash=\"" << current_hash
      << "\"\n";
}

}  // namespace ash
