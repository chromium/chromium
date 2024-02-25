// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DOT_INDICATOR_H_
#define ASH_STYLE_DOT_INDICATOR_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// The indicator which is activated when its corresponding app receives a
// notification. This is thus set invisible as default and should be explicitly
// set to visible by the owner view. The owner of this component is also
// responsible for setting the correct bounds on the app icon.
class ASH_EXPORT DotIndicator : public views::View {
  METADATA_HEADER(DotIndicator, views::View)

 public:
  explicit DotIndicator(SkColor indicator_color);
  DotIndicator(const DotIndicator& other) = delete;
  DotIndicator& operator=(const DotIndicator& other) = delete;
  ~DotIndicator() override;

  void SetColor(SkColor new_color);

  // Sets the bounds of the indicator without shadow.
  void SetIndicatorBounds(gfx::Rect indicator_bounds);

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  const gfx::ShadowValues shadow_values_;
  SkColor indicator_color_;
};

}  // namespace ash

#endif  // ASH_STYLE_DOT_INDICATOR_H_