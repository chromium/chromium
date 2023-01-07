// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_ANNOTATOR_TOOL_H_
#define ASH_PUBLIC_CPP_PROJECTOR_ANNOTATOR_TOOL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

// The annotator tool type.
enum class ASH_PUBLIC_EXPORT AnnotatorToolType {
  kMarker = 0,
  kPen,
  kHighlighter,
  kEraser,
  // TODO(b/196245932) Add support for laser pointer after confirming we are
  // implementing it inside the annotator.
};

// The tool that the annotator will use.
struct ASH_PUBLIC_EXPORT AnnotatorTool {
  static AnnotatorTool FromValue(const base::Value& value);
  base::Value ToValue() const;

  bool operator==(const AnnotatorTool& rhs) const;

  // The color of of the annotator.
  SkColor color = SK_ColorBLACK;

  // The size of the annotator stroke tip.
  int size = 4;

  // The type of the annotator tool.
  AnnotatorToolType type = AnnotatorToolType::kMarker;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_ANNOTATOR_TOOL_H_
