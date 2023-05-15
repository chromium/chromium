// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/jitter_calculator.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/rand_util.h"

namespace ash {

JitterCalculator::JitterCalculator(AmbientJitterConfig config)
    : JitterCalculator(std::move(config),
                       base::BindRepeating(&base::RandInt, 0, 1)) {}

JitterCalculator::JitterCalculator(
    AmbientJitterConfig config,
    RandomBinaryGenerator random_binary_generator)
    : config_(std::move(config)),
      random_binary_generator_(std::move(random_binary_generator)) {
  DCHECK_GE(config_.step_size, 0);
  DCHECK_LE(config_.x_min_translation, 0);
  DCHECK_GE(config_.x_max_translation, 0);
  DCHECK_LE(config_.y_min_translation, 0);
  DCHECK_GE(config_.y_max_translation, 0);
  DCHECK(random_binary_generator_);
}

JitterCalculator::~JitterCalculator() = default;

gfx::Vector2d JitterCalculator::Calculate() {
  AssertCurrentTranslationWithinBounds();
  // Move the translation point randomly one step on each x/y direction.
  int x_increment = config_.step_size * random_binary_generator_.Run();
  int y_increment = x_increment == 0
                        ? config_.step_size
                        : config_.step_size * random_binary_generator_.Run();
  current_translation_.Add(gfx::Vector2d(translate_x_direction * x_increment,
                                         translate_y_direction * y_increment));

  // If the translation point is out of bounds, reset it within bounds and
  // reverse the direction.
  if (current_translation_.x() < config_.x_min_translation) {
    translate_x_direction = 1;
    current_translation_.set_x(config_.x_min_translation);
  } else if (current_translation_.x() > config_.x_max_translation) {
    translate_x_direction = -1;
    current_translation_.set_x(config_.x_max_translation);
  }

  if (current_translation_.y() > config_.y_max_translation) {
    translate_y_direction = -1;
    current_translation_.set_y(config_.y_max_translation);
  } else if (current_translation_.y() < config_.y_min_translation) {
    translate_y_direction = 1;
    current_translation_.set_y(config_.y_min_translation);
  }
  AssertCurrentTranslationWithinBounds();
  return current_translation_;
}

void JitterCalculator::AssertCurrentTranslationWithinBounds() const {
  DCHECK_LE(current_translation_.x(), config_.x_max_translation);
  DCHECK_GE(current_translation_.x(), config_.x_min_translation);
  DCHECK_LE(current_translation_.y(), config_.y_max_translation);
  DCHECK_GE(current_translation_.y(), config_.y_min_translation);
}

}  // namespace ash
