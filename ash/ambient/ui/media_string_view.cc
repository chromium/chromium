// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace ash {

namespace {

// Typography.
constexpr char16_t kMiddleDotSeparator[] = u" • ";

constexpr int kMusicNoteIconSizeDip = 20;

// Returns true if we should show media string for ambient mode on lock-screen
// based on user pref. We should keep the same user policy here as the
// lock-screen media controls to avoid exposing user data on lock-screen without
// consent.
bool ShouldShowOnLockScreen() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(pref);

  return pref->GetBoolean(prefs::kLockScreenMediaControlsEnabled);
}

}  // namespace

MediaStringView::MediaStringView(MediaStringView::Delegate* delegate)
    : delegate_(delegate) {
  SetID(AmbientViewID::kAmbientMediaStringView);
  InitLayout();
}

MediaStringView::~MediaStringView() = default;

void MediaStringView::OnThemeChanged() {
  views::View::OnThemeChanged();
  media_text_->SetShadows(ambient::util::GetTextShadowValues(
      GetColorProvider(), delegate_->GetSettings().text_shadow_elevation));

  const bool dark_mode_enabled =
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
  DCHECK(icon_);
  icon_->SetImage(gfx::CreateVectorIcon(
      kMusicNoteIcon, kMusicNoteIconSizeDip,
      dark_mode_enabled ? delegate_->GetSettings().icon_dark_mode_color
                        : delegate_->GetSettings().icon_light_mode_color));
  DCHECK(media_text_);
  media_text_->SetEnabledColor(
      dark_mode_enabled ? delegate_->GetSettings().text_dark_mode_color
                        : delegate_->GetSettings().text_light_mode_color);
  gfx::Insets shadow_insets =
      gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues(
          nullptr, delegate_->GetSettings().text_shadow_elevation));
  // Compensate the shadow insets to put the text middle align with the icon.
  media_text_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(-shadow_insets.bottom(), 0, -shadow_insets.top(), 0)));
}

void MediaStringView::OnViewBoundsChanged(views::View* observed_view) {
  UpdateMaskLayer();
}

void MediaStringView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock) &&
      !ShouldShowOnLockScreen()) {
    return;
  }

  // Don't show the media string if session info is unavailable, or the active
  // session is marked as sensitive.
  if (!session_info || session_info->is_sensitive) {
    SetVisible(false);
    return;
  }

  bool is_paused = session_info->playback_state ==
                   media_session::mojom::MediaPlaybackState::kPaused;

  // Don't show the media string if paused.
  SetVisible(!is_paused);
}

void MediaStringView::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());

  std::u16string media_string;
  std::u16string middle_dot = kMiddleDotSeparator;
  if (!session_metadata.title.empty() && !session_metadata.artist.empty()) {
    media_string =
        session_metadata.title + middle_dot + session_metadata.artist;
  } else if (!session_metadata.title.empty()) {
    media_string = session_metadata.title;
  } else {
    media_string = session_metadata.artist;
  }

  // Reset text and stop any ongoing animation.
  media_text_->SetText(std::u16string());
  media_text_->layer()->GetAnimator()->StopAnimating();

  media_text_->SetText(media_string);
  media_text_->layer()->SetTransform(gfx::Transform());
  const auto& text_size = media_text_->GetPreferredSize(
      views::SizeBounds(media_text_->width(), {}));
  const int text_width = text_size.width();
  media_text_container_->SetPreferredSize(gfx::Size(
      std::min(kMediaStringMaxWidthDip, text_width), text_size.height()));

  if (NeedToAnimate()) {
    media_text_->SetText(media_string + middle_dot + media_string + middle_dot);
    ScheduleScrolling(/*is_initial=*/true);
  }
}

void MediaStringView::OnImplicitAnimationsCompleted() {
  if (!NeedToAnimate())
    return;

  ScheduleScrolling(/*is_initial=*/false);
}

void MediaStringView::InitLayout() {
  // This view will be drawn on its own layer instead of the layer of
  // |PhotoView| which has a solid black background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  constexpr int kChildSpacingDip = 8;
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kChildSpacingDip);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetPreferredSize(
      gfx::Size(kMusicNoteIconSizeDip, kMusicNoteIconSizeDip));

  media_text_container_ = AddChildView(std::make_unique<views::View>());
  media_text_container_->SetPaintToLayer();
  media_text_container_->layer()->SetFillsBoundsOpaquely(false);
  media_text_container_->layer()->SetMasksToBounds(true);
  auto* text_layout = media_text_container_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  text_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  text_layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  text_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  observed_view_.Observe(media_text_container_.get());

  media_text_ =
      media_text_container_->AddChildView(std::make_unique<views::Label>());
  media_text_->SetPaintToLayer();
  media_text_->layer()->SetFillsBoundsOpaquely(false);

  // Defines the appearance.
  constexpr int kDefaultFontSizeDip = 64;
  constexpr int kMediaStringFontSizeDip = 18;
  media_text_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  media_text_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  media_text_->SetAutoColorReadabilityEnabled(false);
  media_text_->SetFontList(
      ambient::util::GetDefaultFontlist()
          .DeriveWithSizeDelta(kMediaStringFontSizeDip - kDefaultFontSizeDip)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  media_text_->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  BindMediaControllerObserver();
}

void MediaStringView::BindMediaControllerObserver() {
  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  // Service might be unavailable under some test environments.
  if (!service)
    return;

  // Binds to the MediaControllerManager and create a MediaController for the
  // current active media session so that we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());
}

void MediaStringView::UpdateMaskLayer() {
  if (!NeedToAnimate()) {
    media_text_container_->layer()->SetGradientMask(
        gfx::LinearGradient::GetEmpty());
    return;
  }

  // Invalid container width.
  if (media_text_container_->layer()->size().width() == 0) {
    media_text_container_->layer()->SetGradientMask(
        gfx::LinearGradient::GetEmpty());
    return;
  }

  if (!media_text_container_->layer()->HasGradientMask()) {
    float fade_position = static_cast<float>(kMediaStringGradientWidthDip) /
                          media_text_container_->layer()->size().width();
    gfx::LinearGradient gradient_mask(/*angle=*/0);
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(fade_position, 255);
    gradient_mask.AddStep(1 - fade_position, 255);
    gradient_mask.AddStep(1, 0);
    media_text_container_->layer()->SetGradientMask(gradient_mask);
  }
}

bool MediaStringView::NeedToAnimate() const {
  return media_text_
             ->GetPreferredSize(views::SizeBounds(media_text_->width(), {}))
             .width() > media_text_container_->GetPreferredSize().width();
}

gfx::Transform MediaStringView::GetMediaTextTransform(bool is_initial) {
  gfx::Transform transform;
  if (is_initial) {
    // Start animation half way of |media_text_container_|.
    transform.Translate(kMediaStringMaxWidthDip / 2, 0);
  }
  return transform;
}

void MediaStringView::ScheduleScrolling(bool is_initial) {
  if (!GetVisible())
    return;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MediaStringView::StartScrolling,
                                weak_factory_.GetWeakPtr(), is_initial));
}

void MediaStringView::StartScrolling(bool is_initial) {
  ui::Layer* text_layer = media_text_->layer();
  text_layer->SetTransform(GetMediaTextTransform(is_initial));
  {
    // Desired speed is 10 seconds for kMediaStringMaxWidthDip.
    const int text_width =
        media_text_
            ->GetPreferredSize(views::SizeBounds(media_text_->width(), {}))
            .width();
    const int shadow_width =
        gfx::ShadowValue::GetMargin(
            ambient::util::GetTextShadowValues(
                nullptr, delegate_->GetSettings().text_shadow_elevation))
            .width();
    const int start_x = text_layer->GetTargetTransform().To2dTranslation().x();
    const int end_x = -(text_width + shadow_width) / 2;
    const int transform_distance = start_x - end_x;
    const base::TimeDelta kScrollingDuration =
        base::Seconds(10) * transform_distance / kMediaStringMaxWidthDip;

    ui::ScopedLayerAnimationSettings animation(text_layer->GetAnimator());
    animation.SetTransitionDuration(kScrollingDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.AddObserver(this);

    gfx::Transform transform;
    transform.Translate(end_x, 0);
    text_layer->SetTransform(transform);
  }
}

BEGIN_METADATA(MediaStringView)
END_METADATA

}  // namespace ash
