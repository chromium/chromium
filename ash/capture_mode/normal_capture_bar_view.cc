// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/normal_capture_bar_view.h"

#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

constexpr int kSeparatorHeight = 20;

}  // namespace

NormalCaptureBarView::NormalCaptureBarView(
    CaptureModeBehavior* active_behavior) {
  capture_type_view_ =
      AddChildView(std::make_unique<CaptureModeTypeView>(active_behavior));
  views::Separator* separator_1 =
      AddChildView(std::make_unique<views::Separator>());
  capture_source_view_ =
      AddChildView(std::make_unique<CaptureModeSourceView>());
  views::Separator* separator_2 =
      AddChildView(std::make_unique<views::Separator>());

  separator_1->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_1->SetPreferredLength(kSeparatorHeight);
  separator_2->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_2->SetPreferredLength(kSeparatorHeight);

  AppendSettingsButton();
  AppendCloseButton();
}

NormalCaptureBarView::~NormalCaptureBarView() = default;

CaptureModeTypeView* NormalCaptureBarView::GetCaptureTypeView() const {
  return capture_type_view_;
}

CaptureModeSourceView* NormalCaptureBarView::GetCaptureSourceView() const {
  return capture_source_view_;
}

void NormalCaptureBarView::OnCaptureSourceChanged(
    CaptureModeSource new_source) {
  capture_source_view_->OnCaptureSourceChanged(new_source);
}

void NormalCaptureBarView::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_type_view_->OnCaptureTypeChanged(new_type);
  capture_source_view_->OnCaptureTypeChanged(new_type);
}

BEGIN_METADATA(NormalCaptureBarView)
END_METADATA

}  // namespace ash
