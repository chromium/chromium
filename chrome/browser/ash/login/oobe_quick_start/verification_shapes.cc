// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/verification_shapes.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/hash/sha1.h"

namespace ash {
namespace quick_start {

ShapeHolder::ShapeHolder(int firstByte, int secondByte)
    : shape(static_cast<Shape>((firstByte & 0xC0) >> 6)),
      color(static_cast<Color>((firstByte & 0x30) >> 4)),
      digit(std::abs(((firstByte << 8) | secondByte) % 10)) {}

ShapeList GenerateShapes(const std::string& token) {
  auto bytes = base::as_bytes(base::make_span(token.cbegin(), token.cend()));
  base::SHA1Digest digest = base::SHA1HashSpan(bytes);
  return {ShapeHolder{static_cast<int8_t>(digest[0]),
                      static_cast<int8_t>(digest[1])},
          ShapeHolder{static_cast<int8_t>(digest[2]),
                      static_cast<int8_t>(digest[3])},
          ShapeHolder{static_cast<int8_t>(digest[4]),
                      static_cast<int8_t>(digest[5])},
          ShapeHolder{static_cast<int8_t>(digest[6]),
                      static_cast<int8_t>(digest[7])}};
}

}  // namespace quick_start
}  // namespace ash
