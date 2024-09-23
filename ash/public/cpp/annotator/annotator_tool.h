// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_H_
#define ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The annotator tool type.
enum class ASH_PUBLIC_EXPORT AnnotatorToolType {
  kMarker = 0,
  kPen,
  kHighlighter,
  kEraser,
  kToolNone,
  // TODO(b/196245932) Add support for laser pointer after confirming we are
  // implementing it inside the annotator.
};

// The tool that the annotator will use.
struct ASH_PUBLIC_EXPORT AnnotatorTool {
  // Returns the hex value in RGBA format.
  // For example, SK_ColorGREEN -> "00FF00FF".
  std::string GetColorHexString() const;

  // Returns the tool chosen as a string.
  std::string GetToolString() const;

  bool operator==(const AnnotatorTool& rhs) const;

  // The color of of the annotator.
  SkColor color = SK_ColorBLACK;

  // The size of the annotator stroke tip.
  int size = 4;

  // The type of the annotator tool.
  AnnotatorToolType type = AnnotatorToolType::kMarker;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_H_
