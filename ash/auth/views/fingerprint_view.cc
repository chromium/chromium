// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/fingerprint_view.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/auth/views/auth_common.h"
#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/lottie/animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Size of the Fingerprint icon.
constexpr int kFingerprintIconSizeDp = 28;

// Vertical spacing above the fingerprint view.
constexpr int kSpacingTopDp = 28;

// Vertical space between the fingerprint icon and label.
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 18;

// Number of frames and total duration for the fingerprint failed animation.
constexpr int kFingerprintFailedAnimationNumFrames = 45;
constexpr base::TimeDelta kFingerprintFailedAnimationDuration =
    base::Milliseconds(700);

// Delay after a failed attempt before the icon reverts to its default
// 'available' state.
constexpr base::TimeDelta kResetToDefaultIconDelay = base::Milliseconds(1300);

// Duration for which the label displays a temporary message after a gesture
// event.
constexpr base::TimeDelta kResetToDefaultMessageDelay =
    base::Milliseconds(3000);

// Color id's for the fingerprint icon.
constexpr ui::ColorId kFingerprintIconEnabledColorId =
    cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kFingerprintIconDisabledColorId =
    cros_tokens::kCrosSysDisabled;

constexpr float kCheckmarkAnimationPlaybackSpeed = 2.25;

std::unique_ptr<lottie::Animation> GetCheckmarkAnimation(
    ui::ColorProvider* color_provider) {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_LOGIN_ARROW_CHECKMARK_ANIMATION);
  CHECK(lottie_data.has_value());

  cc::SkottieColorMap color_map = cc::SkottieColorMap{
      cc::SkottieMapColor(
          "cros.sys.illo.color2",
          color_provider->GetColor(cros_tokens::kCrosSysPositive)),
      cc::SkottieMapColor(
          "cros.sys.app_base_shaded",
          color_provider->GetColor(cros_tokens::kCrosSysOnSurface)),
  };

  std::unique_ptr<lottie::Animation> animation =
      std::make_unique<lottie::Animation>(
          cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()),
          std::move(color_map));

  animation->SetPlaybackSpeed(kCheckmarkAnimationPlaybackSpeed);

  return animation;
}

}  // namespace

//----------------------- FingerprintView Test API ------------------------

FingerprintView::TestApi::TestApi(FingerprintView* view) : view_(view) {
  CHECK(view_);
}

FingerprintView::TestApi::~TestApi() = default;

bool FingerprintView::TestApi::GetEnabled() const {
  return view_->GetEnabled();
}

void FingerprintView::TestApi::SetEnabled(bool enabled) {
  view_->SetEnabled(enabled);
}

views::Label* FingerprintView::TestApi::GetLabel() {
  return view_->label_;
}
AnimatedRoundedImageView* FingerprintView::TestApi::GetIcon() {
  return view_->icon_;
}

void FingerprintView::TestApi::ShowFirstFrame() {
  view_->icon_->SetAnimationPlayback(
      AnimatedRoundedImageView::Playback::kFirstFrameOnly);
}

void FingerprintView::TestApi::ShowLastFrame() {
  view_->icon_->SetAnimationPlayback(
      AnimatedRoundedImageView::Playback::kLastFrameOnly);
}

FingerprintView* FingerprintView::TestApi::GetView() {
  return view_;
}

FingerprintState FingerprintView::TestApi::GetState() const {
  return view_->state_;
}

//----------------------- FingerprintView ------------------------

FingerprintView::FingerprintView() {
  SetVisible(false);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kSpacingBetweenFingerprintIconAndLabelDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
      gfx::Size(kFingerprintIconSizeDp, kFingerprintIconSizeDp),
      0 /*corner_radius*/));

  lottie_animation_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  lottie_animation_view_->SetImageSize(
      gfx::Size(kFingerprintIconSizeDp, kFingerprintIconSizeDp));
  lottie_animation_view_->SetVisible(false);

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetAutoColorReadabilityEnabled(false);

  // kTextColorId, kTextFont defined in auth_common.h
  label_->SetEnabledColorId(kTextColorId);
  label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(kTextFont));

  label_->SetMultiLine(true);
  label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  label_->GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
}

FingerprintView::~FingerprintView() {
  icon_ = nullptr;
  scoped_animation_observer_.Reset();
  lottie_animation_view_ = nullptr;
  label_ = nullptr;
}

void FingerprintView::SetState(FingerprintState state) {
  if (state_ == state) {
    return;
  }

  if (has_success_) {
    return;
  }

  reset_state_.Stop();
  state_ = state;
  DisplayCurrentState();
  if (NeedA11yAlertFromState()) {
    label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                     /*send_native_event=*/true);
  }
}

void FingerprintView::SetHasPin(bool has_pin) {
  if (has_pin_ == has_pin) {
    return;
  }

  if (has_success_) {
    return;
  }

  has_pin_ = has_pin;
  DisplayCurrentState();
}

void FingerprintView::NotifyAuthSuccess(
    base::OnceClosure on_success_animation_finished) {
  has_success_ = true;
  CHECK(on_success_animation_finished_.is_null());
  on_success_animation_finished_ = std::move(on_success_animation_finished);
  CHECK(GetColorProvider());
  std::unique_ptr<lottie::Animation> animation =
      GetCheckmarkAnimation(GetColorProvider());
  CHECK(animation);
  auto playback_config = lottie::Animation::PlaybackConfig::CreateWithStyle(
      lottie::Animation::Style::kLinear, *animation);
  // Observe animation to know when it finishes playing.
  scoped_animation_observer_.Observe(animation.get());
  lottie_animation_view_->SetAnimatedImage(std::move(animation));
  lottie_animation_view_->Play(playback_config);

  icon_->SetVisible(false);
  lottie_animation_view_->SetVisible(true);
}

void FingerprintView::NotifyAuthFailure() {
  if (has_success_) {
    return;
  }

  SetState(FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT);
  reset_state_.Start(
      FROM_HERE, kResetToDefaultIconDelay,
      base::BindOnce(&FingerprintView::SetState, base::Unretained(this),
                     FingerprintState::AVAILABLE_DEFAULT));
}

void FingerprintView::OnGestureEvent(ui::GestureEvent* event) {
  if (has_success_) {
    return;
  }

  if (event->type() != ui::EventType::kGestureTap) {
    return;
  }

  if (state_ == FingerprintState::AVAILABLE_DEFAULT ||
      state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING ||
      state_ == FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT) {
    SetState(FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
    reset_state_.Start(
        FROM_HERE, kResetToDefaultMessageDelay,
        base::BindOnce(&FingerprintView::SetState, base::Unretained(this),
                       FingerprintState::AVAILABLE_DEFAULT));
  }
}

void FingerprintView::DisplayCurrentState() {
  CHECK(!has_success_);
  if (state_ == FingerprintState::UNAVAILABLE) {
    SetVisible(false);
    return;
  }
  SetVisible(true);
  SetIcon();
  label_->SetText(l10n_util::GetStringUTF16(GetTextIdFromState()));
  label_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(GetA11yTextIdFromState()));
}

void FingerprintView::SetIcon() {
  CHECK(!has_success_);
  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      icon_->SetImageModel(ui::ImageModel::FromVectorIcon(
          kLockScreenFingerprintIcon, GetIconColorIdFromState(),
          kFingerprintIconSizeDp));
      break;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
    case FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT:
      icon_->SetAnimationDecoder(
          std::make_unique<HorizontalImageSequenceAnimationDecoder>(
              *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                  IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
              kFingerprintFailedAnimationDuration,
              kFingerprintFailedAnimationNumFrames),
          AnimatedRoundedImageView::Playback::kSingle);
      break;
    case FingerprintState::UNAVAILABLE:
      NOTREACHED();
  }
}

ui::ColorId FingerprintView::GetIconColorIdFromState() const {
  CHECK(!has_success_);
  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return kFingerprintIconEnabledColorId;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      return kFingerprintIconDisabledColorId;
    case FingerprintState::UNAVAILABLE:
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
    case FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT:
      NOTREACHED();
  }
}

int FingerprintView::GetTextIdFromState() const {
  CHECK(!has_success_);
  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_AVAILABLE;
    case FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_FAILED;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_TOUCH_SENSOR;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_DISABLED_FROM_ATTEMPTS;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      if (has_pin_) {
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PIN_OR_PASSWORD_REQUIRED;
      }
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PASSWORD_REQUIRED;
    case FingerprintState::UNAVAILABLE:
      NOTREACHED();
  }
}

int FingerprintView::GetA11yTextIdFromState() const {
  CHECK(!has_success_);
  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_AVAILABLE;
    case FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_FAILED;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_TOUCH_SENSOR;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_ACCESSIBLE_DISABLED_FROM_ATTEMPTS;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      if (has_pin_) {
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PIN_OR_PASSWORD_REQUIRED;
      }
      return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PASSWORD_REQUIRED;
    case FingerprintState::UNAVAILABLE:
      NOTREACHED();
  }
}

bool FingerprintView::NeedA11yAlertFromState() const {
  CHECK(!has_success_);
  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
      return false;
    case FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT:
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      return true;
    case FingerprintState::UNAVAILABLE:
      CHECK_IS_TEST();
      return false;
  }
}

gfx::Size FingerprintView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int preferred_height = 0;
  if (GetVisible()) {
    preferred_height = kSpacingTopDp + kFingerprintIconSizeDp +
                       kSpacingBetweenFingerprintIconAndLabelDp +
                       label_->GetHeightForWidth(kTextLineWidthDp);
  }
  return gfx::Size(kTextLineWidthDp, preferred_height);
}

void FingerprintView::AnimationCycleEnded(const lottie::Animation* animation) {
  CHECK(has_success_);
  scoped_animation_observer_.Reset();

  if (on_success_animation_finished_) {
    std::move(on_success_animation_finished_).Run();
  }
}

BEGIN_METADATA(FingerprintView)
END_METADATA

}  // namespace ash
