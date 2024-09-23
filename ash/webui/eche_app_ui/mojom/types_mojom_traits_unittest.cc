// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/mojom/types_mojom_traits.h"

#include <map>

#include "base/containers/fixed_flat_map.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {
namespace {
template <typename MojoEnum, typename SourceEnum, size_t N>
void TestToMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<size_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    EXPECT_EQ(
        enum_pair.first,
        (mojo::EnumTraits<MojoEnum, SourceEnum>::ToMojom(enum_pair.second)));
  }
}

template <typename MojoEnum, typename SourceEnum, size_t N>
void TestFromMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<uint32_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    SourceEnum mojo_to_source;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, SourceEnum>::FromMojom(
        enum_pair.first, &mojo_to_source)));
    EXPECT_EQ(mojo_to_source, enum_pair.second);
  }
}
}  // namespace

class TypesMojomTraitsTest : public testing::Test {
 public:
  TypesMojomTraitsTest() = default;
  ~TypesMojomTraitsTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TypesMojomTraitsTest, ScreenBacklightStateMatches) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::ScreenBacklightState,
                                                ash::ScreenBacklightState>({
      {mojom::ScreenBacklightState::ON, ash::ScreenBacklightState::ON},
      {mojom::ScreenBacklightState::OFF, ash::ScreenBacklightState::OFF},
      {mojom::ScreenBacklightState::OFF_AUTO,
       ash::ScreenBacklightState::OFF_AUTO},
  });

  // Ensure mapping between types behaves as expected.
  TestToMojom(enums);
  TestFromMojom(enums);
}

TEST_F(TypesMojomTraitsTest, RejectInvalid) {
  // Create an intentionally garbage value.
  ash::ScreenBacklightState invalid_value =
      static_cast<ash::ScreenBacklightState>(1234);

  ash::ScreenBacklightState output;

  EXPECT_NOTREACHED_DEATH(
      mojo::test::SerializeAndDeserialize<mojom::ScreenBacklightState>(
          invalid_value, output));
}

}  // namespace eche_app
}  // namespace ash
