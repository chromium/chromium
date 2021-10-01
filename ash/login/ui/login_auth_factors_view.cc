// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/callback.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

constexpr int kAuthFactorsViewWidthDp = 204;
constexpr int kSpacingBetweenIconsAndLabelDp = 15;
constexpr int kIconTopSpacingDp = 20;

}  // namespace

class AuthFactorsLabel : public views::Label {
 public:
  AuthFactorsLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary));
    SetMultiLine(true);
  }

  AuthFactorsLabel(const AuthFactorsLabel&) = delete;
  AuthFactorsLabel& operator=(const AuthFactorsLabel&) = delete;

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    node_data->SetName(accessible_name_);
  }

  // views::Label:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary));
  }

  void SetAccessibleName(const std::u16string& name) {
    accessible_name_ = name;
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                             /*send_native_event=*/true);
  }

 private:
  std::u16string accessible_name_;
};

LoginAuthFactorsView::LoginAuthFactorsView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(views::CreateEmptyBorder(kIconTopSpacingDp, 0, 0, 0));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kSpacingBetweenIconsAndLabelDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<AuthIconView>());
  label_ = AddChildView(std::make_unique<AuthFactorsLabel>());
}

LoginAuthFactorsView::~LoginAuthFactorsView() = default;

void LoginAuthFactorsView::AddAuthFactor(
    std::unique_ptr<AuthFactorModel> auth_factor) {
  auth_factor->SetOnStateChangedCallback(base::BindRepeating(
      &LoginAuthFactorsView::UpdateState, base::Unretained(this)));
  auth_factors_.push_back(std::move(auth_factor));
  UpdateState();
}

void LoginAuthFactorsView::UpdateState() {
  if (auth_factors_.empty())
    return;
  // TODO(crbug.com/1233614) Add support for multiple auth factors.
  auto& auth_factor = auth_factors_[0];

  SetVisible(auth_factor->GetAuthFactorState() !=
             AuthFactorState::kUnavailable);
  auth_factor->UpdateIcon(icon_);
  label_->SetText(auth_factor->GetLabel());
  label_->SetAccessibleName(auth_factor->GetAccessibleName());

  if (auth_factor->ShouldAnnounceLabel())
    FireAlert();
}

// views::View:
gfx::Size LoginAuthFactorsView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  size.set_width(kAuthFactorsViewWidthDp);
  return size;
}

// views::View:
void LoginAuthFactorsView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP &&
      event->type() != ui::ET_GESTURE_TAP_DOWN)
    return;

  // TODO(crbug.com/1233614) Route tap events from respective icons instead.
  for (const auto& factor : auth_factors_) {
    factor->OnTapEvent();
  }
}

// views::View:
void LoginAuthFactorsView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // TODO(crbug.com/1233614) Update all visible icons.
  auth_factors_[0]->UpdateIcon(icon_);
}

void LoginAuthFactorsView::FireAlert() {
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                   /*send_native_event=*/true);
}

}  // namespace ash
