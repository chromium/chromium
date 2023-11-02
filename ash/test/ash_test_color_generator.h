// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_COLOR_GENERATOR_H_
#define ASH_TEST_ASH_TEST_COLOR_GENERATOR_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// A helper class to return a color. Usually used for solid-colored icons.
class AshTestColorGenerator {
 public:
  explicit AshTestColorGenerator(SkColor default_color);
  AshTestColorGenerator(const AshTestColorGenerator&) = delete;
  AshTestColorGenerator& operator=(const AshTestColorGenerator&) = delete;
  ~AshTestColorGenerator();

  SkColor default_color() const { return default_color_; }

  // This method guarantees that two consequent callings get different colors.
  SkColor GetAlternativeColor();

 private:
  const SkColor default_color_;

  // The next color to be returned by `GetAlternativeColor()`.
  size_t next_color_index_ = 0;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_COLOR_GENERATOR_H_
