// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/jitter_calculator.h"

#include <limits>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {
namespace {

using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;

}  // namespace

class JitterCalculatorTest : public ::testing::Test {
 protected:
  void set_random_binary(int random_binary) { random_binary_ = random_binary; }

  JitterCalculator::RandomBinaryGenerator GetRandomBinaryGenerator() {
    return base::BindRepeating(&JitterCalculatorTest::GenerateRandomBinary,
                               base::Unretained(this));
  }

 private:
  int GenerateRandomBinary() { return random_binary_; }

  int random_binary_ = 1;
};

TEST_F(JitterCalculatorTest, CalculateHonorsConfig) {
  AmbientJitterConfig config{.step_size = 2,
                             .x_min_translation = -10,
                             .x_max_translation = 10,
                             .y_min_translation = -20,
                             .y_max_translation = 20};

  JitterCalculator calculator(config, GetRandomBinaryGenerator());
  int max_x_translation_observed = std::numeric_limits<int>::min();
  int min_x_translation_observed = std::numeric_limits<int>::max();
  int max_y_translation_observed = std::numeric_limits<int>::min();
  int min_y_translation_observed = std::numeric_limits<int>::max();
  gfx::Vector2d previous_translation;
  for (int i = 0; i < 200; ++i, set_random_binary(i % 2)) {
    gfx::Vector2d translation = calculator.Calculate();
    gfx::Vector2d step(translation);
    step.Subtract(previous_translation);

    max_x_translation_observed =
        std::max(translation.x(), max_x_translation_observed);
    min_x_translation_observed =
        std::min(translation.x(), min_x_translation_observed);
    max_y_translation_observed =
        std::max(translation.y(), max_y_translation_observed);
    min_y_translation_observed =
        std::min(translation.y(), min_y_translation_observed);

    ASSERT_THAT(step.x(), AnyOf(Eq(0), Eq(2), Eq(-2)));
    ASSERT_THAT(step.y(), AnyOf(Eq(0), Eq(2), Eq(-2)));

    previous_translation = translation;
  }
  EXPECT_THAT(max_x_translation_observed, Eq(10));
  EXPECT_THAT(min_x_translation_observed, Eq(-10));
  EXPECT_THAT(max_y_translation_observed, Eq(20));
  EXPECT_THAT(min_y_translation_observed, Eq(-20));
}

TEST_F(JitterCalculatorTest, AllowsFor0MinMaxTranslation) {
  AmbientJitterConfig config{.step_size = 2,
                             .x_min_translation = 0,
                             .x_max_translation = 10,
                             .y_min_translation = -20,
                             .y_max_translation = 0};

  JitterCalculator calculator(config, GetRandomBinaryGenerator());
  int min_x_translation_observed = std::numeric_limits<int>::max();
  int max_y_translation_observed = std::numeric_limits<int>::min();
  for (int i = 0; i < 200; ++i, set_random_binary(i % 2)) {
    gfx::Vector2d translation = calculator.Calculate();

    min_x_translation_observed =
        std::min(translation.x(), min_x_translation_observed);
    max_y_translation_observed =
        std::max(translation.y(), max_y_translation_observed);
  }
  EXPECT_THAT(min_x_translation_observed, Eq(0));
  EXPECT_THAT(max_y_translation_observed, Eq(0));
}

TEST_F(JitterCalculatorTest, SetConfigToZero) {
  AmbientJitterConfig config{.step_size = 0};
  JitterCalculator jitter_calculator_(config);
  for (int i = 0; i < 200; ++i) {
    ASSERT_TRUE(jitter_calculator_.Calculate().IsZero());
  }
}

}  // namespace ash
