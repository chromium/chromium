// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_VERIFICATION_SHAPES_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_VERIFICATION_SHAPES_H_

#include <array>
#include <cstdint>
#include <string>

namespace ash::quick_start {

// See internal go/oobe-verification-shapes for details.
enum class Shape {
  kCircle = 0,
  kDiamond = 1,
  kTriangle = 2,
  kSquare = 3,
};

enum class Color {
  kBlue = 0,
  kRed = 1,
  kGreen = 2,
  kYellow = 3,
};

struct ShapeHolder {
  ShapeHolder() = delete;

  ShapeHolder(int firstByte, int secondByte);

  const Shape shape;
  const Color color;
  const int8_t digit;
};

using ShapeList = std::array<ShapeHolder, 4>;

ShapeList GenerateShapes(const std::string& token);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_VERIFICATION_SHAPES_H_
