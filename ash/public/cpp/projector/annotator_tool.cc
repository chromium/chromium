// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/annotator_tool.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace ash {

namespace {

const char kToolColor[] = "color";
const char kToolSize[] = "size";
const char kToolType[] = "tool";

// Returns the hex value in RGBA format.
// For example, SK_ColorGREEN -> "00FF00FF".
std::string ConvertColorToHexString(SkColor color) {
  uint8_t alpha = SkColorGetA(color);
  uint8_t red = SkColorGetR(color);
  uint8_t green = SkColorGetG(color);
  uint8_t blue = SkColorGetB(color);
  uint8_t bytes[] = {red, green, blue, alpha};
  return base::HexEncode(bytes);
}

// Converts the RGBA hex string to ARGB, then to an SkColor.
// For example, "000000FF" -> "SK_ColorBLACK".
// Returns red if conversion fails.
SkColor ConvertHexStringToColor(const std::string& rgba_hex) {
  const size_t kHexColorLength = 8;
  const size_t kRgbaLength = 6;
  if (rgba_hex.length() < kHexColorLength) {
    return SK_ColorRED;
  }
  uint32_t argb_color;
  // Shift the alpha value to the front of the hex string.
  const bool success = base::HexStringToUInt(
      rgba_hex.substr(kRgbaLength) + rgba_hex.substr(0, kRgbaLength),
      &argb_color);
  return success ? argb_color : SK_ColorRED;
}

std::string ConvertToolTypeToString(AnnotatorToolType type) {
  switch (type) {
    case AnnotatorToolType::kMarker:
      return "marker";
    case AnnotatorToolType::kPen:
      return "pen";
    case AnnotatorToolType::kHighlighter:
      return "highlighter";
    case AnnotatorToolType::kEraser:
      return "eraser";
  }
}

AnnotatorToolType ConvertStringToToolType(const std::string& type) {
  if (type == "marker")
    return AnnotatorToolType::kMarker;
  if (type == "pen")
    return AnnotatorToolType::kPen;
  if (type == "highlighter")
    return AnnotatorToolType::kHighlighter;
  if (type == "eraser")
    return AnnotatorToolType::kEraser;
  NOTREACHED();
  return AnnotatorToolType::kMarker;
}

}  // namespace

// static
AnnotatorTool AnnotatorTool::FromValue(const base::Value& value) {
  DCHECK(value.is_dict());
  DCHECK(value.FindKey(kToolColor));
  DCHECK(value.FindKey(kToolColor)->is_string());
  DCHECK(value.FindKey(kToolSize));
  DCHECK(value.FindKey(kToolSize)->is_int());
  DCHECK(value.FindKey(kToolType));
  DCHECK(value.FindKey(kToolType)->is_string());
  AnnotatorTool t;
  t.color = ConvertHexStringToColor(*(value.FindStringPath(kToolColor)));
  t.size = *(value.FindIntPath(kToolSize));
  t.type = ConvertStringToToolType(*(value.FindStringPath(kToolType)));
  return t;
}

base::Value AnnotatorTool::ToValue() const {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetKey(kToolColor, base::Value(ConvertColorToHexString(color)));
  val.SetKey(kToolSize, base::Value(size));
  val.SetKey(kToolType, base::Value(ConvertToolTypeToString(type)));
  return val;
}

bool AnnotatorTool::operator==(const AnnotatorTool& rhs) const {
  return rhs.color == color && rhs.size == size && rhs.type == type;
}

}  // namespace ash
