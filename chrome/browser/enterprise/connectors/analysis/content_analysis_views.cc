// Copyright 2025 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_views.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace enterprise_connectors {

namespace {

constexpr int kSideImageSize = 24;
constexpr gfx::Insets kSideImageInsets(8);

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
      features::IsRoundedIconsEnabled() ? vector_icons::kDomainIcon
                                        : vector_icons::kBusinessOldIcon,
      gfx::kPlaceholderColor, kSideImageSize));
  SetBorder(views::CreateEmptyBorder(kSideImageInsets));
}

void ContentAnalysisSideIconImageView::Update() {
  if (!GetWidget()) {
    return;
  }
  SetImage(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kDomainIcon
                                        : vector_icons::kBusinessOldIcon,
      delegate()->GetSideImageLogoColor(), kSideImageSize));
  if (delegate()->is_result()) {
    SetBackground(
        views::CreatePillBackground(delegate()->GetSideImageBackgroundColor()));
  }
}

void ContentAnalysisSideIconImageView::OnThemeChanged() {
  views::ImageView::OnThemeChanged();
  Update();
}

BEGIN_METADATA(ContentAnalysisSideIconSpinnerView)
END_METADATA
}  // namespace enterprise_connectors
