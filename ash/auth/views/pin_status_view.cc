// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_status_view.h"

#include "ash/ash_export.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/style/typography.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

// Distance between the top of the view and the label.
constexpr int kTopLabelDistanceDp = 28;

}  // namespace

namespace ash {

PinStatusView::TestApi::TestApi(PinStatusView* view) : view_(view) {}
PinStatusView::TestApi::~TestApi() = default;

const std::u16string& PinStatusView::TestApi::GetCurrentText() const {
  return view_->text_label_->GetText();
}

raw_ptr<PinStatusView> PinStatusView::TestApi::GetView() {
  return view_;
}

PinStatusView::PinStatusView(const std::u16string& text) {
  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  };

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  // Add space.
  AddVerticalSpace(this, kTopLabelDistanceDp);

  // Add text.
  text_label_ = new views::Label(text, views::style::CONTEXT_LABEL,
                                 views::style::STYLE_PRIMARY);
  text_label_->SetMultiLine(true);
  text_label_->SizeToFit(kTextLineWidthDp);
  text_label_->SetEnabledColorId(kTextColorId);
  text_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(kTextFont));
  decorate_label(text_label_);
  AddChildView(text_label_.get());
}

PinStatusView::~PinStatusView() {
  text_label_ = nullptr;
}

gfx::Size PinStatusView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      kTopLabelDistanceDp + text_label_->GetHeightForWidth(kTextLineWidthDp);
  return gfx::Size(kTextLineWidthDp, preferred_height);
}

void PinStatusView::SetText(const std::u16string& text_str) {
  text_label_->SetText(text_str);
}

BEGIN_METADATA(PinStatusView)
END_METADATA

}  // namespace ash
