// Copyright 2025 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_views.h"

#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace enterprise_connectors {

namespace {

constexpr int kSideImageSize = 24;
constexpr gfx::Insets kSideImageInsets(8);

// A simple background class to show a colored circle behind the side icon once
// the scanning is done.
class CircleBackground : public views::Background {
 public:
  explicit CircleBackground(ui::ColorId color) { SetColor(color); }

  CircleBackground(const CircleBackground&) = delete;
  CircleBackground& operator=(const CircleBackground&) = delete;

  ~CircleBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color().ResolveToSkColor(view->GetColorProvider()));
    canvas->DrawCircle(center, radius, flags);
  }

  void OnViewThemeChanged(views::View* view) override { view->SchedulePaint(); }
};

}  // namespace

ContentAnalysisBaseView::ContentAnalysisBaseView(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ContentAnalysisBaseView::Delegate* ContentAnalysisBaseView::delegate() {
  return delegate_;
}

BEGIN_METADATA(ContentAnalysisTopImageView)
END_METADATA

void ContentAnalysisTopImageView::Update() {
  if (!GetWidget()) {
    return;
  }
  SetImage(ui::ImageModel::FromResourceId(delegate()->GetTopImageId()));
}

void ContentAnalysisTopImageView::OnThemeChanged() {
  views::ImageView::OnThemeChanged();
  Update();
}

BEGIN_METADATA(ContentAnalysisSideIconImageView)
END_METADATA

ContentAnalysisSideIconImageView::ContentAnalysisSideIconImageView(
    Delegate* delegate)
    : ContentAnalysisBaseView(delegate) {
  SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, gfx::kPlaceholderColor, kSideImageSize));
  SetBorder(views::CreateEmptyBorder(kSideImageInsets));
}

void ContentAnalysisSideIconImageView::Update() {
  if (!GetWidget()) {
    return;
  }
  SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                          delegate()->GetSideImageLogoColor(),
                                          kSideImageSize));
  if (delegate()->is_result()) {
    SetBackground(std::make_unique<CircleBackground>(
        delegate()->GetSideImageBackgroundColor()));
  }
}

void ContentAnalysisSideIconImageView::OnThemeChanged() {
  views::ImageView::OnThemeChanged();
  Update();
}

BEGIN_METADATA(ContentAnalysisSideIconSpinnerView)
END_METADATA

void ContentAnalysisSideIconSpinnerView::Update() {
  if (delegate()->is_result()) {
    parent()->RemoveChildView(this);
    delete this;
  }
}

void ContentAnalysisSideIconSpinnerView::OnThemeChanged() {
  views::Throbber::OnThemeChanged();
  Update();
}

}  // namespace enterprise_connectors
