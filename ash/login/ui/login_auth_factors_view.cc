// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/style/ash_color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kAuthFactorsViewWidthDp = 204;
constexpr int kSpacingBetweenIconsAndLabelDp = 15;
constexpr int kIconTopSpacingDp = 20;

}  // namespace

LoginAuthFactorsView::LoginAuthFactorsView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(views::CreateEmptyBorder(kIconTopSpacingDp, 0, 0, 0));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kSpacingBetweenIconsAndLabelDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<AuthIconView>());

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  label_->SetMultiLine(true);
}

LoginAuthFactorsView::~LoginAuthFactorsView() = default;

void LoginAuthFactorsView::AddAuthFactor(
    std::unique_ptr<AuthFactorModel> auth_factor) {
  auth_factors_.push_back(std::move(auth_factor));
  UpdateState();
}

void LoginAuthFactorsView::UpdateState() {
  if (auth_factors_.empty())
    return;
  // TODO(crbug.com/1233614) Add support for multiple auth factors.
  auto& auth_factor = auth_factors_[0];

  auth_factor->UpdateIcon(icon_);
  label_->SetText(auth_factor->GetLabel());
}

// views::View:
gfx::Size LoginAuthFactorsView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  size.set_width(kAuthFactorsViewWidthDp);
  return size;
}

}  // namespace ash
