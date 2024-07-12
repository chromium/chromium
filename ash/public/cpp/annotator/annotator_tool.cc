// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/annotator/annotator_tool.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace ash {

std::string AnnotatorTool::GetColorHexString() const {
  uint8_t alpha = SkColorGetA(color);
  uint8_t red = SkColorGetR(color);
  uint8_t green = SkColorGetG(color);
  uint8_t blue = SkColorGetB(color);
  uint8_t bytes[] = {red, green, blue, alpha};
  return base::HexEncode(bytes);
}

std::string AnnotatorTool::GetToolString() const {
  switch (type) {
    case AnnotatorToolType::kMarker:
      return "marker";
    case AnnotatorToolType::kPen:
      return "pen";
    case AnnotatorToolType::kHighlighter:
      return "highlighter";
    case AnnotatorToolType::kEraser:
      return "eraser";
    case AnnotatorToolType::kToolNone:
      return "";
  }
}

bool AnnotatorTool::operator==(const AnnotatorTool& rhs) const {
  return rhs.color == color && rhs.size == size && rhs.type == type;
}

}  // namespace ash
