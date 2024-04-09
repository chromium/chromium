// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/playlist_image_button.h"

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

constexpr int kSinglePlaylistViewWidth = 72;
constexpr int kIconSize = 20;
constexpr int kMediaActionIconSpacing = 6;
constexpr int kSelectedCurvycutoutSpacing = 4;

std::unique_ptr<lottie::Animation> GetEqualizerAnimation() {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_FOCUS_MODE_EQUALIZER_LIGHT_ANIMATION);
  CHECK(lottie_data.has_value());

  return std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()));
}

}  // namespace

PlaylistImageButton::PlaylistImageButton(const gfx::ImageSkia& image,
                                         PressedCallback callback)
    : views::Button(std::move(callback)) {
  // We want to show the stop icon or the equalizer animation for the mouse
  // moving inside of or outside of this view when the playlist is playing.
  // Thus, to let `OnMouseEntered` and `OnMouseExited` be triggered for the
  // mouse event, we call the function here.
  SetNotifyEnterExitOnChild(true);
  gfx::Size preferred_size(kSinglePlaylistViewWidth, kSinglePlaylistViewWidth);
  SetPreferredSize(preferred_size);

  image_view_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_->SetImage(image);
  image_view_->SetImageSize(preferred_size);

  media_action_icon_ = AddChildView(std::make_unique<views::ImageView>());

  selected_curvycutout_icon_ =
      AddChildView(std::make_unique<views::ImageView>());
  selected_curvycutout_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kSelectedIcon, cros_tokens::kCrosSysPrimary, kIconSize));

  lottie_animation_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  lottie_animation_view_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  lottie_animation_view_->SetAnimatedImage(GetEqualizerAnimation());

  SetIsPlaying(false);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
}

PlaylistImageButton::~PlaylistImageButton() = default;

void PlaylistImageButton::OnMouseEntered(const ui::MouseEvent& event) {
  Button::OnMouseEntered(event);
  UpdateVisibility();
}

void PlaylistImageButton::OnMouseExited(const ui::MouseEvent& event) {
  Button::OnMouseExited(event);
  UpdateVisibility();
}

views::ProposedLayout PlaylistImageButton::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    layouts.host_size = GetPreferredSize();
    return layouts;
  }
  auto bounds = GetContentsBounds();
  layouts.child_layouts.emplace_back(image_view_.get(),
                                     image_view_->GetVisible(), bounds);

  auto media_action_bounds =
      gfx::Rect(bounds.right() - kIconSize - kMediaActionIconSpacing,
                bounds.bottom() - kIconSize - kMediaActionIconSpacing,
                kIconSize, kIconSize);
  layouts.child_layouts.emplace_back(media_action_icon_.get(),
                                     media_action_icon_->GetVisible(),
                                     media_action_bounds);
  layouts.child_layouts.emplace_back(lottie_animation_view_.get(),
                                     lottie_animation_view_->GetVisible(),
                                     media_action_bounds);

  layouts.child_layouts.emplace_back(
      selected_curvycutout_icon_.get(),
      selected_curvycutout_icon_->GetVisible(),
      gfx::Rect(kSelectedCurvycutoutSpacing, kSelectedCurvycutoutSpacing,
                kIconSize, kIconSize));
  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  return layouts;
}

void PlaylistImageButton::SetIsPlaying(bool is_playing) {
  is_playing_ = is_playing;
  is_playing_ ? lottie_animation_view_->Play() : lottie_animation_view_->Stop();
  media_action_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      is_playing_ ? kFocusModeStopCircleIcon : kFocusModePlayCircleIcon,
      SK_ColorWHITE, kIconSize));
  UpdateVisibility();
}

void PlaylistImageButton::OnSetTooltipText(const std::u16string& tooltip_text) {
  // Set the tooltip text for `image_view_` to show the tooltip when hovering on
  // it.
  image_view_->SetTooltipText(tooltip_text);
}

void PlaylistImageButton::UpdateVisibility() {
  selected_curvycutout_icon_->SetVisible(is_playing_);

  const bool is_animation_visible = is_playing_ && !IsMouseHovered();
  lottie_animation_view_->SetVisible(is_animation_visible);
  media_action_icon_->SetVisible(!is_animation_visible);
}

BEGIN_METADATA(PlaylistImageButton)
END_METADATA

}  // namespace ash
