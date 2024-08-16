// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_factors_view.h"

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/animated_auth_factors_label_wrapper.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

using AuthFactorState = AuthFactorModel::AuthFactorState;

constexpr int kAuthFactorsViewWidthDp = 280;
constexpr int kSpacingBetweenIconsAndLabelDp = 8;
constexpr int kIconTopSpacingDp = 10;
constexpr int kArrowButtonSizeDp = 32;
constexpr base::TimeDelta kErrorTimeout = base::Seconds(3);
constexpr float kCheckmarkAnimationPlaybackSpeed = 2.25;

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
      if (auth_factor.has_permanent_error_display_timed_out()) {
        return PrioritizedAuthFactorViewState::kErrorBackground;
      }

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
  if (auth_factors.empty()) {
    return nullptr;
  }

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

std::unique_ptr<lottie::Animation> GetCheckmarkAnimation(
    ui::ColorProvider* color_provider) {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_LOGIN_ARROW_CHECKMARK_ANIMATION);
  CHECK(lottie_data.has_value());

  cc::SkottieColorMap color_map = cc::SkottieColorMap{
      cc::SkottieMapColor("cros.sys.illo.color2",
                          color_provider->GetColor(AuthIconView::GetColorId(
                              AuthIconView::Status::kPositive))),
      cc::SkottieMapColor("cros.sys.app_base_shaded",
                          color_provider->GetColor(AuthIconView::GetColorId(
                              AuthIconView::Status::kPrimary))),
  };

  std::unique_ptr<lottie::Animation> animation =
      std::make_unique<lottie::Animation>(
          cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()),
          std::move(color_map));

  animation->SetPlaybackSpeed(kCheckmarkAnimationPlaybackSpeed);

  return animation;
}

}  // namespace

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
  return view_->label_wrapper_->label();
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
    base::RepeatingClosure on_click_to_enter_callback,
    base::RepeatingCallback<void(bool)>
        on_auth_factor_is_hiding_password_changed_callback)
    : on_click_to_enter_callback_(on_click_to_enter_callback),
      on_auth_factor_is_hiding_password_changed_callback_(
          on_auth_factor_is_hiding_password_changed_callback) {
  DCHECK(on_click_to_enter_callback);
  DCHECK(on_auth_factor_is_hiding_password_changed_callback);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kIconTopSpacingDp, 0, 0, 0)));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

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
  arrow_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER));

  arrow_nudge_animation_ =
      arrow_icon_container_->AddChildView(std::make_unique<AuthIconView>());
  arrow_nudge_animation_->set_on_tap_or_click_callback(base::BindRepeating(
      &LoginAuthFactorsView::RelayArrowButtonPressed, base::Unretained(this)));

  SetArrowVisibility(false);

  // TODO(crbug.com/1233614): Rename kLockScreenFingerprintSuccessIcon once the
  // feature flag is removed and FingerprintView no longer needs this.
  checkmark_icon_ = AddChildView(std::make_unique<AuthIconView>());
  checkmark_icon_->SetVisible(false);

  label_wrapper_ =
      AddChildView(std::make_unique<AnimatedAuthFactorsLabelWrapper>());
  label_wrapper_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingBetweenIconsAndLabelDp, 0, 0, 0));
  label_wrapper_->label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
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
  if (can_use_pin == AuthFactorModel::can_use_pin()) {
    return;
  }

  AuthFactorModel::set_can_use_pin(can_use_pin);
  UpdateState();
}

bool LoginAuthFactorsView::ShouldHidePasswordField() {
  return should_hide_password_field_;
}

void LoginAuthFactorsView::UpdateState() {
  AuthFactorModel* active_auth_factor =
      GetHighestPriorityAuthFactor(auth_factors_);
  if (!active_auth_factor) {
    return;
  }

  PrioritizedAuthFactorViewState state =
      GetPrioritizedAuthFactorViewState(*active_auth_factor);
  if (state == PrioritizedAuthFactorViewState::kUnavailable) {
    return;
  }

  if (state != PrioritizedAuthFactorViewState::kErrorForeground) {
    error_timer_.Stop();
  }

  UpdateShouldHidePasswordField(*active_auth_factor);

  int ready_label_id;
  size_t num_factors_in_error_background_state;
  switch (state) {
    case PrioritizedAuthFactorViewState::kAuthenticated:
      // An auth factor has successfully authenticated. Show a green checkmark.
      ShowCheckmark();
      if (LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLogin) {
        label_wrapper_->SetLabelTextAndAccessibleName(
            IDS_AUTH_FACTOR_LABEL_SIGNED_IN, IDS_AUTH_FACTOR_LABEL_SIGNED_IN);
      } else {
        label_wrapper_->SetLabelTextAndAccessibleName(
            IDS_AUTH_FACTOR_LABEL_UNLOCKED, IDS_AUTH_FACTOR_LABEL_UNLOCKED);
      }

      // Clear focus so that the focus on arrow button does not jump to another
      // element after the view transitions.
      GetFocusManager()->ClearFocus();

      return;
    case PrioritizedAuthFactorViewState::kClickRequired:
      // An auth factor requires a click to enter. Show arrow button.
      ShowArrowButton();
      label_wrapper_->SetLabelTextAndAccessibleName(
          IDS_AUTH_FACTOR_LABEL_CLICK_TO_ENTER,
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
      label_wrapper_->SetLabelTextAndAccessibleName(ready_label_id,
                                                    ready_label_id,
                                                    /*animate=*/true);
      // TODO(crbug.com/1233614): Should FireAlert() be called here?
      FireAlert();
      return;
    case PrioritizedAuthFactorViewState::kAvailable:
      // At least one auth factor is available, but none are ready. Show first
      // available auth factor.
      ShowSingleAuthFactor(active_auth_factor);
      label_wrapper_->SetLabelTextAndAccessibleName(
          active_auth_factor->GetLabelId(),
          active_auth_factor->GetAccessibleNameId(), /*animate=*/true);
      if (active_auth_factor->ShouldAnnounceLabel()) {
        FireAlert();
      }
      return;
    case PrioritizedAuthFactorViewState::kErrorForeground:
      // An auth factor has either a temporary or permanent error to show. Show
      // the error for a period of time.

      // Do not replace the current error if an error is already showing.
      if (error_timer_.IsRunning()) {
        return;
      }

      error_timer_.Start(FROM_HERE, kErrorTimeout,
                         base::BindOnce(&LoginAuthFactorsView::OnErrorTimeout,
                                        base::Unretained(this)));

      ShowSingleAuthFactor(active_auth_factor);
      label_wrapper_->SetLabelTextAndAccessibleName(
          active_auth_factor->GetLabelId(),
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

      num_factors_in_error_background_state = base::ranges::count(
          auth_factors_, PrioritizedAuthFactorViewState::kErrorBackground,
          [](const auto& factor) {
            return GetPrioritizedAuthFactorViewState(*factor);
          });

      if (num_factors_in_error_background_state == 1) {
        label_wrapper_->SetLabelTextAndAccessibleName(
            active_auth_factor->GetLabelId(),
            active_auth_factor->GetAccessibleNameId());
      } else {
        label_wrapper_->SetLabelTextAndAccessibleName(GetDefaultLabelId(),
                                                      GetDefaultLabelId());
      }
      return;
    case PrioritizedAuthFactorViewState::kUnavailable:
      NOTREACHED();
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
  const bool arrow_button_was_visible = arrow_button_->GetVisible();
  auth_factor_icon_row_->SetVisible(false);
  SetArrowVisibility(false);
  if (arrow_button_was_visible) {
    checkmark_icon_->SetLottieAnimation(
        GetCheckmarkAnimation(GetColorProvider()));
  } else {
    checkmark_icon_->SetIcon(kLockScreenFingerprintSuccessIcon,
                             AuthIconView::Status::kPositive);
  }
  checkmark_icon_->SetVisible(true);
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
  }

  if (ready_factor_count == 1u) {
    return ready_factor->GetLabelId();
  }

  // Multiple auth factors are ready.
  switch (ready_factors) {
    case AuthFactorType::kSmartLock | AuthFactorType::kFingerprint:
      return IDS_AUTH_FACTOR_LABEL_UNLOCK_METHOD_SELECTION;
  }

  NOTREACHED();
}

int LoginAuthFactorsView::GetDefaultLabelId() const {
  return AuthFactorModel::can_use_pin()
             ? IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED
             : IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED;
}

// views::View:
gfx::Size LoginAuthFactorsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::SizeBounds content_available_size(available_size);
  content_available_size.set_width(kAuthFactorsViewWidthDp);
  gfx::Size size = views::View::CalculatePreferredSize(content_available_size);
  size.set_width(kAuthFactorsViewWidthDp);
  return size;
}

// views::View:
void LoginAuthFactorsView::OnThemeChanged() {
  views::View::OnThemeChanged();

  for (const auto& factor : auth_factors_) {
    factor->OnThemeChanged();
  }

  arrow_nudge_animation_->SetCircleImage(
      kArrowButtonSizeDp / 2,
      GetColorProvider()->GetColor(kColorAshHairlineBorderColor));
}

void LoginAuthFactorsView::FireAlert() {
  label_wrapper_->label()->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
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
    ArrowButtonPressed(ui::MouseEvent(ui::EventType::kMousePressed,
                                      gfx::Point(), gfx::Point(),
                                      base::TimeTicks::Now(), 0, 0));
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
    arrow_button_->RequestFocus();
  } else {
    arrow_nudge_animation_->StopAnimating();
    arrow_button_->StopAnimating();
  }
}

void LoginAuthFactorsView::UpdateShouldHidePasswordField(
    const AuthFactorModel& active_auth_factor) {
  PrioritizedAuthFactorViewState state =
      GetPrioritizedAuthFactorViewState(active_auth_factor);

  // At the moment, Smart Lock is the only auth factor which needs to hide
  // the password field, and it does so only during states kClickRequired
  // and kAuthenticated.
  bool should_hide_password_field =
      active_auth_factor.GetType() == AuthFactorType::kSmartLock &&
      (state == PrioritizedAuthFactorViewState::kClickRequired ||
       state == PrioritizedAuthFactorViewState::kAuthenticated);

  if (should_hide_password_field == should_hide_password_field_) {
    return;
  }
  should_hide_password_field_ = should_hide_password_field;

  on_auth_factor_is_hiding_password_changed_callback_.Run(
      should_hide_password_field);
}

BEGIN_METADATA(LoginAuthFactorsView)
END_METADATA

}  // namespace ash
