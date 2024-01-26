// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_example.h"

#include <memory>

#include "cc/paint/paint_canvas.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"

namespace {

class GradientView : public views::View {
 public:
  METADATA_HEADER(GradientView);

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    SkColor left = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);
    SkColor right = SkColorSetARGB(0xff, 0, 0, 0);
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(width(), 0), gfx::Point(0, height()), left, right));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }
};

BEGIN_METADATA(GradientView, views::View)
END_METADATA

}  // namespace

WatermarkExample::WatermarkExample() : ExampleBase("Watermark") {}

void WatermarkExample::CreateExampleView(views::View* container) {
  container->SetUseDefaultFillLayout(true);
  container->AddChildView(std::make_unique<GradientView>());
  container->AddChildView(std::make_unique<enterprise_watermark::WatermarkView>(
      "Private! Confidential"));
}

WatermarkExample::~WatermarkExample() = default;
