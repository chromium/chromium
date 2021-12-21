// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

constexpr int kAuthFactorsViewWidthDp = 204;
constexpr int kSpacingBetweenIconsAndLabelDp = 8;
constexpr int kIconTopSpacingDp = 10;
constexpr int kArrowButtonSizeDp = 32;
constexpr base::TimeDelta kErrorTimeout = base::Seconds(3);

// The values of this enum should be nearly the same as the values of
// AuthFactorState, except instead of kErrorTemporary and kErrorPermanent, we
// have kErrorForeground and kErrorBackground.
//
// Foreground/background here refers to whether or not the error has already
// been displayed to the user. Permanent errors, which can't be recovered from,
// start in the foreground and then transition to the background after having
// been displayed. Temporary errors, on the other hand, start in the foreground
// and then transition to a non-error state after display.
//
// The idea is to provide separation of concerns: AuthFactorModel is concerned
// with the type of error being shown, but LoginAuthFactorsView is concerned
// with how to show the error. When deciding how to prioritize which states to
// show, what matters is whether the error is currently in the foreground or
// background, not whether the underlying error state is temporary or permanent.
//
// DO NOT change the relative ordering of these enum values. The values
// assigned here correspond to the priority of these states. For example, if
// LoginAuthFactorsView has one auth factor in the kClickRequired state and
// one auth factor in the kReady state, then it will prioritize showing the
// kClickRequired state since it's assigned a higher priority.
enum class PrioritizedAuthFactorViewState {
  // All auth factors are unavailable, and LoginAuthFactorsView should not be
  // visible.
  kUnavailable = 0,
  // All auth factors are either unavailable or have permanent errors that have
  // already been displayed.
  kErrorBackground = 1,
  // There is at least one auth factor that is available, but it requires extra
  // steps to authenticate.
  kAvailable = 2,
  // There is at least one auth factor that is ready to authenticate.
  kReady = 3,
  // An auth factor has an error message to display.
  kErrorForeground = 4,
  // An auth factor requires a tap or click as the last step in its
  // authentication flow.
  kClickRequired = 5,
  // Authentication is complete.
  kAuthenticated = 6,
};

PrioritizedAuthFactorViewState GetPrioritizedAuthFactorViewState(
    const AuthFactorModel& auth_factor) {
  switch (auth_factor.GetAuthFactorState()) {
    case AuthFactorState::kUnavailable:
      return PrioritizedAuthFactorViewState::kUnavailable;
    case AuthFactorState::kErrorPermanent:
      if (auth_factor.has_permanent_error_display_timed_out())
        return PrioritizedAuthFactorViewState::kErrorBackground;

      return PrioritizedAuthFactorViewState::kErrorForeground;
    case AuthFactorState::kAvailable:
      return PrioritizedAuthFactorViewState::kAvailable;
    case AuthFactorState::kReady:
      return PrioritizedAuthFactorViewState::kReady;
    case AuthFactorState::kErrorTemporary:
      return PrioritizedAuthFactorViewState::kErrorForeground;
    case AuthFactorState::kClickRequired:
      return PrioritizedAuthFactorViewState::kClickRequired;
    case AuthFactorState::kAuthenticated:
      return PrioritizedAuthFactorViewState::kAuthenticated;
  }
}

// Return the AuthFactorModel whose state has the highest priority. "Priority"
// here roughly corresponds with how close the given state is to completing the
// auth flow. E.g. kAvailable < kReady because there are fewer steps to complete
// authentication for a Ready auth factor. The highest priority auth factor's
// state determines the behavior of LoginAuthFactorsView.
AuthFactorModel* GetHighestPriorityAuthFactor(
    const std::vector<std::unique_ptr<AuthFactorModel>>& auth_factors) {
  if (auth_factors.empty())
    return nullptr;

  // PrioritizedAuthFactorViewState enum values are assigned so that the
  // highest numerical value corresponds to the highest priority.
  auto compare_by_priority = [](const std::unique_ptr<AuthFactorModel>& a,
                                const std::unique_ptr<AuthFactorModel>& b) {
    return static_cast<int>(GetPrioritizedAuthFactorViewState(*a)) <
           static_cast<int>(GetPrioritizedAuthFactorViewState(*b));
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

AuthIconView* LoginAuthFactorsView::TestApi::arrow_nudge_animation() {
  return view_->arrow_nudge_animation_;
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

  SetBoxLayout(this);

  auth_factor_icon_row_ = AddChildView(std::make_unique<views::View>());
  auto* animating_layout = auth_factor_icon_row_->SetLayoutManager(
      std::make_unique<views::AnimatingLayoutManager>());
  animating_layout->SetBoundsAnimationMode(
      views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis);
  animating_layout->SetTargetLayoutManager(
      std::make_unique<views::FlexLayout>());

  arrow_icon_container_ = AddChildView(std::make_unique<views::View>());
  arrow_icon_container_->SetUseDefaultFillLayout(true);

  arrow_button_container_ =
      arrow_icon_container_->AddChildView(std::make_unique<views::View>());
  SetBoxLayout(arrow_button_container_);

  arrow_button_ =
      arrow_button_container_->AddChildView(std::make_unique<ArrowButtonView>(
          base::BindRepeating(&LoginAuthFactorsView::ArrowButtonPressed,
                              base::Unretained(this)),
          kArrowButtonSizeDp));
  arrow_button_->SetInstallFocusRingOnFocus(true);
  views::InstallCircleHighlightPathGenerator(arrow_button_);
  arrow_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER));

  arrow_nudge_animation_ =
      arrow_icon_container_->AddChildView(std::make_unique<AuthIconView>());
  arrow_nudge_animation_->SetCircleImage(
      kArrowButtonSizeDp / 2, AshColorProvider::Get()->GetControlsLayerColor(
                                  AshColorProvider::ControlsLayerType::
                                      kControlBackgroundColorInactive));

  arrow_nudge_animation_->set_on_tap_or_click_callback(base::BindRepeating(
      &LoginAuthFactorsView::RelayArrowButtonPressed, base::Unretained(this)));

  SetArrowVisibility(false);

  // TODO(crbug.com/1233614): Rename kLockScreenFingerprintSuccessIcon once the
  // feature flag is removed and FingerprintView no longer needs this.
  checkmark_icon_ = AddChildView(std::make_unique<AuthIconView>());
  checkmark_icon_->SetIcon(kLockScreenFingerprintSuccessIcon,
                           AuthIconView::Color::kPositive);
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
      /*update_state_callback=*/base::BindRepeating(
          &LoginAuthFactorsView::UpdateState, base::Unretained(this)));
  icon->set_on_tap_or_click_callback(base::BindRepeating(
      &AuthFactorModel::HandleTapOrClick, base::Unretained(auth_factor.get())));
  auth_factors_.push_back(std::move(auth_factor));
  UpdateState();
}

void LoginAuthFactorsView::SetCanUsePin(bool can_use_pin) {
  if (can_use_pin == AuthFactorModel::can_use_pin())
    return;

  AuthFactorModel::set_can_use_pin(can_use_pin);
  UpdateState();
}

void LoginAuthFactorsView::UpdateState() {
  AuthFactorModel* active_auth_factor =
      GetHighestPriorityAuthFactor(auth_factors_);
  if (!active_auth_factor) {
    SetVisible(false);
    return;
  }

  PrioritizedAuthFactorViewState state =
      GetPrioritizedAuthFactorViewState(*active_auth_factor);
  if (state == PrioritizedAuthFactorViewState::kUnavailable) {
    SetVisible(false);
    return;
  }
  SetVisible(true);

  if (state != PrioritizedAuthFactorViewState::kErrorForeground) {
    error_timer_.Stop();
  }

  int ready_label_id;
  size_t num_factors_in_error_background_state;
  switch (state) {
    case PrioritizedAuthFactorViewState::kAuthenticated:
      // An auth factor has successfully authenticated. Show a green checkmark.
      ShowCheckmark();
      if (LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLogin) {
        SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_SIGNED_IN,
                                      IDS_AUTH_FACTOR_LABEL_SIGNED_IN);
      } else {
        SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_UNLOCKED,
                                      IDS_AUTH_FACTOR_LABEL_UNLOCKED);
      }
      return;
    case PrioritizedAuthFactorViewState::kClickRequired:
      // An auth factor requires a click to enter. Show arrow button.
      // TODO(crbug.com/1233614): collapse password/pin
      ShowArrowButton();
      SetLabelTextAndAccessibleName(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER,
                                    IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER);
      FireAlert();

      // Dismiss any errors in the background.
      OnErrorTimeout();
      return;
    case PrioritizedAuthFactorViewState::kReady:
      // One or more auth factors is in the Ready state. Show the ready auth
      // factors.
      ShowReadyAndDisabledAuthFactors();
      ready_label_id = GetReadyLabelId();
      SetLabelTextAndAccessibleName(ready_label_id, ready_label_id);
      // TODO(crbug.com/1233614): Should FireAlert() be called here?
      FireAlert();
      return;
    case PrioritizedAuthFactorViewState::kAvailable:
      // At least one auth factor is available, but none are ready. Show first
      // available auth factor.
      ShowSingleAuthFactor(active_auth_factor);
      SetLabelTextAndAccessibleName(active_auth_factor->GetLabelId(),
                                    active_auth_factor->GetAccessibleNameId());
      if (active_auth_factor->ShouldAnnounceLabel()) {
        FireAlert();
      }
      return;
    case PrioritizedAuthFactorViewState::kErrorForeground:
      // An auth factor has either a temporary or permanent error to show. Show
      // the error for a period of time.

      // Do not replace the current error if an error is already showing.
      if (error_timer_.IsRunning())
        return;

      error_timer_.Start(FROM_HERE, kErrorTimeout,
                         base::BindOnce(&LoginAuthFactorsView::OnErrorTimeout,
                                        base::Unretained(this)));

      ShowSingleAuthFactor(active_auth_factor);
      SetLabelTextAndAccessibleName(active_auth_factor->GetLabelId(),
                                    active_auth_factor->GetAccessibleNameId());
      if (active_auth_factor->ShouldAnnounceLabel()) {
        FireAlert();
      }
      return;
    case PrioritizedAuthFactorViewState::kErrorBackground:
      // Any auth factors that were available have errors that cannot be
      // resolved, and those errors have already been displayed in the
      // foreground. Show the "disabled" icons and instruct the user to enter
      // their password.
      ShowReadyAndDisabledAuthFactors();

      num_factors_in_error_background_state = std::count_if(
          auth_factors_.begin(), auth_factors_.end(), [](const auto& factor) {
            return GetPrioritizedAuthFactorViewState(*factor) ==
                   PrioritizedAuthFactorViewState::kErrorBackground;
          });

      if (num_factors_in_error_background_state == 1) {
        SetLabelTextAndAccessibleName(
            active_auth_factor->GetLabelId(),
            active_auth_factor->GetAccessibleNameId());
      } else {
        SetLabelTextAndAccessibleName(GetDefaultLabelId(), GetDefaultLabelId());
      }
      return;
    case PrioritizedAuthFactorViewState::kUnavailable:
      NOTREACHED();
      return;
  }
}

void LoginAuthFactorsView::ShowArrowButton() {
  auth_factor_icon_row_->SetVisible(false);
  checkmark_icon_->SetVisible(false);
  SetArrowVisibility(true);
}

void LoginAuthFactorsView::ShowSingleAuthFactor(AuthFactorModel* auth_factor) {
  auth_factor_icon_row_->SetVisible(true);
  checkmark_icon_->SetVisible(false);
  SetArrowVisibility(false);
  for (auto& factor : auth_factors_) {
    factor->SetVisible(factor.get() == auth_factor);
  }
}

void LoginAuthFactorsView::ShowReadyAndDisabledAuthFactors() {
  auth_factor_icon_row_->SetVisible(true);
  checkmark_icon_->SetVisible(false);
  SetArrowVisibility(false);

  for (auto& factor : auth_factors_) {
    PrioritizedAuthFactorViewState state =
        GetPrioritizedAuthFactorViewState(*factor);
    factor->SetVisible(state == PrioritizedAuthFactorViewState::kReady ||
                       state ==
                           PrioritizedAuthFactorViewState::kErrorBackground);
  }
}

void LoginAuthFactorsView::ShowCheckmark() {
  auth_factor_icon_row_->SetVisible(false);
  checkmark_icon_->SetVisible(true);
  SetArrowVisibility(false);
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
    return GetDefaultLabelId();
  }

  if (ready_factor_count == 1u)
    return ready_factor->GetLabelId();

  // Multiple auth factors are ready.
  switch (ready_factors) {
    case AuthFactorType::kSmartLock | AuthFactorType::kFingerprint:
      return IDS_AUTH_FACTOR_LABEL_UNLOCK_METHOD_SELECTION;
  }

  NOTREACHED();
  return GetDefaultLabelId();
}

int LoginAuthFactorsView::GetDefaultLabelId() const {
  return AuthFactorModel::can_use_pin()
             ? IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED
             : IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED;
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
  arrow_nudge_animation_->SetVisible(false);
  arrow_nudge_animation_->StopAnimating();
  arrow_button_->StopAnimating();

  if (on_click_to_enter_callback_) {
    arrow_button_->EnableLoadingAnimation(true);
    on_click_to_enter_callback_.Run();
  }
}

void LoginAuthFactorsView::RelayArrowButtonPressed() {
  if (arrow_button_) {
    ArrowButtonPressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                      gfx::Point(), base::TimeTicks::Now(), 0,
                                      0));
  }
}

void LoginAuthFactorsView::OnErrorTimeout() {
  for (auto& factor : auth_factors_) {
    // If additional errors occur during the error timeout, then mark all
    // errors timed out instead of trying to queue them. The user can still get
    // the error messages by clicking on the icons.
    if (GetPrioritizedAuthFactorViewState(*factor) ==
        PrioritizedAuthFactorViewState::kErrorForeground) {
      factor->HandleErrorTimeout();
    }
  }
}

void LoginAuthFactorsView::SetBoxLayout(views::View* parent_view) {
  auto* layout =
      parent_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kSpacingBetweenIconsAndLabelDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

void LoginAuthFactorsView::SetArrowVisibility(bool is_visible) {
  arrow_icon_container_->SetVisible(is_visible);
  arrow_button_->SetVisible(is_visible);
  arrow_nudge_animation_->SetVisible(is_visible);

  if (is_visible) {
    arrow_button_->EnableLoadingAnimation(false);
    arrow_button_->RunTransformAnimation();
    arrow_nudge_animation_->RunNudgeAnimation();
  } else {
    arrow_nudge_animation_->StopAnimating();
    arrow_button_->StopAnimating();
  }
}

}  // namespace ash
