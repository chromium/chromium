// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
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
constexpr int kSpacingBetweenIconsDp = 28;
constexpr int kIconSizeDp = 32;

// Return the AuthFactorModel whose AuthFactorState has the highest priority.
// "Priority" here roughly corresponds with how close the given state is to
// completing the auth flow. E.g. kAvailable < kReady because there are fewer
// steps to complete authentication for a Ready auth factor. The highest
// priority auth factor's state determines the behavior of LoginAuthFactorsView.
AuthFactorModel* GetHighestPriorityAuthFactor(
    const std::vector<std::unique_ptr<AuthFactorModel>>& auth_factors) {
  if (auth_factors.empty())
    return nullptr;

  // AuthFactorState's enum values are assigned so that the highest numerical
  // value corresponds to the highest priority.
  auto compare_by_priority = [](const std::unique_ptr<AuthFactorModel>& a,
                                const std::unique_ptr<AuthFactorModel>& b) {
    return static_cast<int>(a->GetAuthFactorState()) <
           static_cast<int>(b->GetAuthFactorState());
  };

  auto& max = *std::max_element(auth_factors.begin(), auth_factors.end(),
                                compare_by_priority);
  return max.get();
}

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

views::View* LoginAuthFactorsView::TestApi::auth_factor_icon_row() {
  return view_->auth_factor_icon_row_;
}

ArrowButtonView* LoginAuthFactorsView::TestApi::arrow_button() {
  return view_->arrow_button_;
}

AuthIconView* LoginAuthFactorsView::TestApi::checkmark_icon() {
  return view_->checkmark_icon_;
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

  auth_factor_icon_row_ = AddChildView(std::make_unique<views::View>());
  auto* icon_row_layout = auth_factor_icon_row_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kSpacingBetweenIconsDp));
  icon_row_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  icon_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  arrow_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&LoginAuthFactorsView::ArrowButtonPressed,
                          base::Unretained(this)),
      kArrowButtonSizeDp));
  arrow_button_->SetInstallFocusRingOnFocus(true);
  views::InstallCircleHighlightPathGenerator(arrow_button_);
  arrow_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER));
  arrow_button_->SetVisible(false);

  checkmark_icon_ = AddChildView(std::make_unique<AuthIconView>());
  checkmark_icon_->SetImage(gfx::CreateVectorIcon(
      kLockScreenFingerprintSuccessIcon, kIconSizeDp,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPositive)));
  checkmark_icon_->SetVisible(false);

  label_ = AddChildView(std::make_unique<AuthFactorsLabel>());
}

LoginAuthFactorsView::~LoginAuthFactorsView() = default;

void LoginAuthFactorsView::AddAuthFactor(
    std::unique_ptr<AuthFactorModel> auth_factor) {
  auto* icon =
      auth_factor_icon_row_->AddChildView(std::make_unique<AuthIconView>());
  auth_factor->Init(
      icon,
      /*on_state_changed_callback=*/base::BindRepeating(
          &LoginAuthFactorsView::UpdateState, base::Unretained(this)));
  icon->set_on_tap_or_click_callback(
      base::BindRepeating(&AuthFactorModel::OnTapOrClickEvent,
                          base::Unretained(auth_factor.get())));
  auth_factors_.push_back(std::move(auth_factor));
  UpdateState();
}

void LoginAuthFactorsView::UpdateState() {
  AuthFactorModel* active_auth_factor =
      GetHighestPriorityAuthFactor(auth_factors_);

  if (!active_auth_factor || active_auth_factor->GetAuthFactorState() ==
                                 AuthFactorState::kUnavailable) {
    SetVisible(false);
    return;
  }
  SetVisible(true);

  int ready_label_id;
  switch (active_auth_factor->GetAuthFactorState()) {
    case AuthFactorState::kAuthenticated:
      // An auth factor has successfully authenticated. Show a green checkmark.
      ShowCheckmark();
      SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_UNLOCKED,
                                    IDS_AUTH_FACTOR_LABEL_UNLOCKED);
      return;
    case AuthFactorState::kClickRequired:
      // An auth factor requires a click to enter. Show arrow button.
      // TODO(crbug.com/1233614): collapse password/pin
      ShowArrowButton();
      SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER,
                                    IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER);
      FireAlert();
      return;
    case AuthFactorState::kReady:
      // One or more auth factors is in the Ready state. Show the ready auth
      // factors.
      // TODO(crbug.com/1233614): show disabled auth factors
      ShowReadyAuthFactors();
      ready_label_id = GetReadyLabelId();
      SetLabelTextAndAccessibleName(ready_label_id, ready_label_id);
      // TODO(crbug.com/1233614): Should FireAlert() be called here?
      FireAlert();
      return;
    case AuthFactorState::kErrorTemporary:
      // An auth factor has an error to show temporarily. Show the error for a
      // period of time.
      // TODO(crbug.com/1233614): Handle error timeout
      FALLTHROUGH;
    case AuthFactorState::kAvailable:
      // At least one auth factor is available, but none are ready. Show first
      // available auth factor.
      ShowSingleAuthFactor(active_auth_factor);
      SetLabelTextAndAccessibleName(active_auth_factor->GetLabelId(),
                                    active_auth_factor->GetAccessibleNameId());
      if (active_auth_factor->ShouldAnnounceLabel()) {
        FireAlert();
      }
      return;
    case AuthFactorState::kErrorPermanent:
      // Any auth factors that were available have errors that cannot be
      // resolved. Show the "disabled" icons and instruct the user to enter
      // their password.
      // TODO(crbug.com/1233614): show disabled auth factors
      auth_factor_icon_row_->SetVisible(false);
      arrow_button_->SetVisible(false);
      checkmark_icon_->SetVisible(false);
      SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_UNLOCK_PASSWORD,
                                    IDS_AUTH_FACTOR_LABEL_UNLOCK_PASSWORD);
      return;
    case AuthFactorState::kUnavailable:
      NOTREACHED();
      return;
  }
}

void LoginAuthFactorsView::ShowArrowButton() {
  auth_factor_icon_row_->SetVisible(false);
  arrow_button_->SetVisible(true);
  checkmark_icon_->SetVisible(false);
  arrow_button_->EnableLoadingAnimation(false);
}

void LoginAuthFactorsView::ShowSingleAuthFactor(AuthFactorModel* auth_factor) {
  auth_factor_icon_row_->SetVisible(true);
  arrow_button_->SetVisible(false);
  checkmark_icon_->SetVisible(false);
  for (auto& factor : auth_factors_) {
    factor->SetVisible(factor.get() == auth_factor);
  }
}

void LoginAuthFactorsView::ShowReadyAuthFactors() {
  auth_factor_icon_row_->SetVisible(true);
  arrow_button_->SetVisible(false);
  checkmark_icon_->SetVisible(false);
  for (auto& factor : auth_factors_) {
    factor->SetVisible(factor->GetAuthFactorState() == AuthFactorState::kReady);
  }
}

void LoginAuthFactorsView::ShowCheckmark() {
  auth_factor_icon_row_->SetVisible(false);
  arrow_button_->SetVisible(false);
  checkmark_icon_->SetVisible(true);
  // TODO(crbug.com/1233614): If transitioning from Click Required state, show
  // animation.
}

void LoginAuthFactorsView::SetLabelTextAndAccessibleName(
    int label_id,
    int accessible_name_id) {
  label_->SetText(l10n_util::GetStringUTF16(label_id));
  label_->SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
}

int LoginAuthFactorsView::GetReadyLabelId() const {
  AuthFactorTypeBits ready_factors = 0;
  unsigned ready_factor_count = 0u;
  AuthFactorModel* ready_factor = nullptr;
  for (const auto& factor : auth_factors_) {
    if (factor->GetAuthFactorState() == AuthFactorState::kReady) {
      ready_factors = ready_factors | factor->GetType();
      ready_factor_count++;
      ready_factor = factor.get();
    }
  }

  if (ready_factor_count == 0u) {
    LOG(ERROR) << "GetReadyLabelId() called without any ready auth factors.";
    NOTREACHED();
    return IDS_AUTH_FACTOR_LABEL_UNLOCK_PASSWORD;
  }

  if (ready_factor_count == 1u)
    return ready_factor->GetLabelId();

  // Multiple auth factors are ready.
  switch (ready_factors) {
    case AuthFactorType::kSmartLock | AuthFactorType::kFingerprint:
      return IDS_AUTH_FACTOR_LABEL_UNLOCK_METHOD_SELECTION;
  }

  NOTREACHED();
  return IDS_AUTH_FACTOR_LABEL_UNLOCK_PASSWORD;
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

  for (const auto& factor : auth_factors_) {
    factor->OnThemeChanged();
  }
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
