// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/callback.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

constexpr int kAuthFactorsViewWidthDp = 204;
constexpr int kSpacingBetweenIconsAndLabelDp = 15;
constexpr int kIconTopSpacingDp = 20;
constexpr int kArrowButtonSizeDp = 32;

}  // namespace

class AuthFactorsLabel : public views::Label {
 public:
  AuthFactorsLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorSecondary));
    SetMultiLine(true);
    SizeToFit(kAuthFactorsViewWidthDp);
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

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = views::View::CalculatePreferredSize();
    size.set_width(kAuthFactorsViewWidthDp);
    return size;
  }

  void SetAccessibleName(const std::u16string& name) {
    accessible_name_ = name;
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                             /*send_native_event=*/true);
  }

 private:
  std::u16string accessible_name_;
};

LoginAuthFactorsView::TestApi::TestApi(LoginAuthFactorsView* view)
    : view_(view) {}

LoginAuthFactorsView::TestApi::~TestApi() = default;

void LoginAuthFactorsView::TestApi::UpdateState() {
  return view_->UpdateState();
}

std::vector<std::unique_ptr<AuthFactorModel>>&
LoginAuthFactorsView::TestApi::auth_factors() {
  return view_->auth_factors_;
}

views::Label* LoginAuthFactorsView::TestApi::label() {
  return view_->label_;
}

LoginAuthFactorsView::LoginAuthFactorsView(
    base::RepeatingClosure on_click_to_enter)
    : on_click_to_enter_callback_(on_click_to_enter) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(views::CreateEmptyBorder(kIconTopSpacingDp, 0, 0, 0));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kSpacingBetweenIconsAndLabelDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<AuthIconView>());

  arrow_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&LoginAuthFactorsView::ArrowButtonPressed,
                          base::Unretained(this)),
      kArrowButtonSizeDp));
  arrow_button_->SetInstallFocusRingOnFocus(true);
  views::InstallCircleHighlightPathGenerator(arrow_button_);
  arrow_button_->SetVisible(false);

  label_ = AddChildView(std::make_unique<AuthFactorsLabel>());
}

LoginAuthFactorsView::~LoginAuthFactorsView() = default;

void LoginAuthFactorsView::AddAuthFactor(
    std::unique_ptr<AuthFactorModel> auth_factor) {
  auth_factor->SetOnStateChangedCallback(base::BindRepeating(
      &LoginAuthFactorsView::UpdateState, base::Unretained(this)));
  // Associate the tap or click callback to the newly added auth factor iff
  // there was no prior auth factor in the model.
  if (auth_factors_.empty()) {
    // TODO(crbug.com/1233614): Associate auth factor model with correct icon
    // when showing multiple icons.
    icon_->set_on_tap_or_click_callback(
        base::BindRepeating(&AuthFactorModel::OnTapOrClickEvent,
                            base::Unretained(auth_factor.get())));
  }
  auth_factors_.push_back(std::move(auth_factor));
  UpdateState();
}

void LoginAuthFactorsView::UpdateState() {
  if (auth_factors_.empty()) {
    SetVisible(false);
    return;
  }
  // TODO(crbug.com/1233614) Add support for multiple auth factors.
  auto& auth_factor = auth_factors_[0];

  SetVisible(auth_factor->GetAuthFactorState() !=
             AuthFactorState::kUnavailable);

  if (auth_factor->GetAuthFactorState() == AuthFactorState::kClickRequired) {
    icon_->SetVisible(false);
    arrow_button_->SetVisible(true);
    arrow_button_->EnableLoadingAnimation(false);
    const std::u16string msg =
        l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER);
    label_->SetText(msg);
    label_->SetAccessibleName(msg);
    FireAlert();
  } else {
    icon_->SetVisible(true);
    arrow_button_->SetVisible(false);
    arrow_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(auth_factor->GetAccessibleNameId()));
    auth_factor->UpdateIcon(icon_);
    label_->SetText(l10n_util::GetStringUTF16(auth_factor->GetLabelId()));
    label_->SetAccessibleName(
        l10n_util::GetStringUTF16(auth_factor->GetAccessibleNameId()));

    if (auth_factor->ShouldAnnounceLabel())
      FireAlert();
  }
}

// views::View:
gfx::Size LoginAuthFactorsView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  size.set_width(kAuthFactorsViewWidthDp);
  return size;
}

// views::View:
void LoginAuthFactorsView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // TODO(crbug.com/1233614) Update all visible icons.
  if (auth_factors_.empty())
    return;

  auth_factors_[0]->UpdateIcon(icon_);
}

void LoginAuthFactorsView::FireAlert() {
  label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                   /*send_native_event=*/true);
}

void LoginAuthFactorsView::ArrowButtonPressed(const ui::Event& event) {
  if (on_click_to_enter_callback_) {
    arrow_button_->EnableLoadingAnimation(true);
    on_click_to_enter_callback_.Run();
  }
}

}  // namespace ash
