// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_view.h"

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

GlanceablesView::GlanceablesView() {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  welcome_label_ = AddChildView(std::make_unique<views::Label>());
  welcome_label_->SetAutoColorReadabilityEnabled(false);
  // TODO(crbug.com/1353119): Use color provider and move to OnThemeChanged().
  welcome_label_->SetEnabledColor(SK_ColorWHITE);
  // TODO(crbug.com/1353119): Localized text.
  welcome_label_->SetText(u"Placeholder");
}

GlanceablesView::~GlanceablesView() = default;

}  // namespace ash
