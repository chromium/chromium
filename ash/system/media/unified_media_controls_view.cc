// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/media/unified_media_controls_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr int kMediaControlsCornerRadius = 8;
constexpr int kMediaControlsViewPadding = 16;
constexpr int kMediaButtonsPadding = 8;
constexpr int kMediaButtonIconSize = 20;
constexpr int kArtworkCornerRadius = 4;
constexpr int kTitleRowHeight = 20;
constexpr int kTrackTitleFontSizeIncrease = 1;

constexpr gfx::Insets kTrackColumnInsets = gfx::Insets(1, 0, 1, 0);
constexpr gfx::Insets kMediaControlsViewInsets = gfx::Insets(8, 8, 8, 12);

constexpr gfx::Size kArtworkSize = gfx::Size(40, 40);
constexpr gfx::Size kMediaButtonSize = gfx::Size(32, 32);

gfx::Size ScaleSizeToFitView(const gfx::Size& size,
                             const gfx::Size& view_size) {
  // If |size| is too big in either dimension or two small in both
  // dimensions, scale it appropriately.
  if ((size.width() > view_size.width() ||
       size.height() > view_size.height()) ||
      (size.width() < view_size.width() &&
       size.height() < view_size.height())) {
    const float scale =
        std::min(view_size.width() / static_cast<float>(size.width()),
                 view_size.height() / static_cast<float>(size.height()));
    return gfx::ScaleToFlooredSize(size, scale);
  }

  return size;
}

const gfx::VectorIcon& GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return vector_icons::kMediaPreviousTrackIcon;
    case MediaSessionAction::kPause:
      return vector_icons::kPauseIcon;
    case MediaSessionAction::kNextTrack:
      return vector_icons::kMediaNextTrackIcon;
    case MediaSessionAction::kPlay:
      return vector_icons::kPlayArrowIcon;

    // Actions that are not supported.
    case MediaSessionAction::kSeekBackward:
    case MediaSessionAction::kSeekForward:
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
    case MediaSessionAction::kSwitchAudioDevice:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return gfx::kNoneIcon;
}

}  // namespace

UnifiedMediaControlsView::MediaActionButton::MediaActionButton(
    views::ButtonListener* listener,
    MediaSessionAction action,
    const base::string16& accessible_name)
    : views::ImageButton(listener) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetPreferredSize(kMediaButtonSize);
  SetAction(action, accessible_name);

  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

void UnifiedMediaControlsView::MediaActionButton::SetAction(
    MediaSessionAction action,
    const base::string16& accessible_name) {
  set_tag(static_cast<int>(action));
  SetTooltipText(accessible_name);
  SetImage(views::Button::STATE_NORMAL,
           CreateVectorIcon(
               GetVectorIconForMediaAction(action), kMediaButtonIconSize,
               AshColorProvider::Get()->GetContentLayerColor(
                   AshColorProvider::ContentLayerType::kIconColorPrimary)));
}

std::unique_ptr<views::InkDrop>
UnifiedMediaControlsView::MediaActionButton::CreateInkDrop() {
  auto ink_drop = TrayPopupUtils::CreateInkDrop(this);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

std::unique_ptr<views::InkDropHighlight>
UnifiedMediaControlsView::MediaActionButton::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(this);
}

std::unique_ptr<views::InkDropRipple>
UnifiedMediaControlsView::MediaActionButton::CreateInkDropRipple() const {
  return TrayPopupUtils::CreateInkDropRipple(
      TrayPopupInkDropStyle::FILL_BOUNDS, this,
      GetInkDropCenterBasedOnLastEvent());
}

UnifiedMediaControlsView::UnifiedMediaControlsView(
    UnifiedMediaControlsController* controller)
    : views::Button(this), controller_(controller) {
  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kMediaControlsCornerRadius));
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMediaControlsViewInsets,
      kMediaControlsViewPadding));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto artwork_view = std::make_unique<views::ImageView>();
  artwork_view->SetPreferredSize(kArtworkSize);
  artwork_view_ = AddChildView(std::move(artwork_view));
  artwork_view_->SetVisible(false);

  auto track_column = std::make_unique<views::View>();
  track_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kTrackColumnInsets));

  auto title_row = std::make_unique<views::View>();
  auto* title_row_layout =
      title_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));
  title_row_layout->set_minimum_cross_axis_size(kTitleRowHeight);

  auto config_label = [](views::Label* label) {
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
  };

  auto title_label = std::make_unique<views::Label>();
  config_label(title_label.get());
  title_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  title_label->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(
          kTrackTitleFontSizeIncrease));
  title_label_ = title_row->AddChildView(std::move(title_label));

  auto drop_down_icon = std::make_unique<views::ImageView>();
  drop_down_icon->SetPreferredSize(gfx::Size(kTitleRowHeight, kTitleRowHeight));
  drop_down_icon->SetImage(CreateVectorIcon(
      kUnifiedMenuMoreIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  title_row->AddChildView(std::move(drop_down_icon));

  title_row_layout->SetFlexForView(title_label_, 1);
  track_column->AddChildView(std::move(title_row));

  auto artist_label = std::make_unique<views::Label>();
  config_label(artist_label.get());
  artist_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  artist_label_ = track_column->AddChildView(std::move(artist_label));

  box_layout->SetFlexForView(AddChildView(std::move(track_column)), 1);

  auto button_row = std::make_unique<views::View>();
  button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kMediaButtonsPadding));

  button_row->AddChildView(std::make_unique<MediaActionButton>(
      this, MediaSessionAction::kPreviousTrack,
      l10n_util::GetStringUTF16(
          IDS_ASH_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK)));

  play_pause_button_ =
      button_row->AddChildView(std::make_unique<MediaActionButton>(
          this, MediaSessionAction::kPause,
          l10n_util::GetStringUTF16(IDS_ASH_MEDIA_NOTIFICATION_ACTION_PAUSE)));

  button_row->AddChildView(std::make_unique<MediaActionButton>(
      this, MediaSessionAction::kNextTrack,
      l10n_util::GetStringUTF16(IDS_ASH_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK)));

  button_row_ = AddChildView(std::move(button_row));
}

void UnifiedMediaControlsView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (sender == this) {
    controller_->OnMediaControlsViewClicked();
    return;
  }

  controller_->PerformAction(
      media_message_center::GetActionFromButtonTag(*sender));
}

void UnifiedMediaControlsView::SetIsPlaying(bool playing) {
  if (playing) {
    play_pause_button_->SetAction(
        MediaSessionAction::kPause,
        l10n_util::GetStringUTF16(IDS_ASH_MEDIA_NOTIFICATION_ACTION_PAUSE));
  } else {
    play_pause_button_->SetAction(
        MediaSessionAction::kPlay,
        l10n_util::GetStringUTF16(IDS_ASH_MEDIA_NOTIFICATION_ACTION_PLAY));
  }
}

void UnifiedMediaControlsView::SetArtwork(
    base::Optional<gfx::ImageSkia> artwork) {
  if (!artwork.has_value()) {
    artwork_view_->SetImage(nullptr);
    artwork_view_->SetVisible(false);
    artwork_view_->InvalidateLayout();
    return;
  }

  artwork_view_->SetVisible(true);
  gfx::Size image_size = ScaleSizeToFitView(artwork->size(), kArtworkSize);
  artwork_view_->SetImageSize(image_size);
  artwork_view_->SetImage(*artwork);

  Layout();
  artwork_view_->SetClipPath(GetArtworkClipPath());
}

void UnifiedMediaControlsView::SetTitle(const base::string16& title) {
  title_label_->SetText(title);
}

void UnifiedMediaControlsView::SetArtist(const base::string16& artist) {
  artist_label_->SetText(artist);
}

void UnifiedMediaControlsView::UpdateActionButtonAvailability(
    const base::flat_set<MediaSessionAction>& enabled_actions) {
  bool should_invalidate = false;
  for (views::View* child : button_row_->children()) {
    views::Button* button = static_cast<views::Button*>(child);
    bool should_show = base::Contains(
        enabled_actions, media_message_center::GetActionFromButtonTag(*button));

    should_invalidate |= should_show != button->GetVisible();
    button->SetVisible(should_show);
  }

  if (should_invalidate)
    button_row_->InvalidateLayout();
}

SkPath UnifiedMediaControlsView::GetArtworkClipPath() {
  // Calculate image bounds since we might need to draw this when image is
  // not visible (i.e. when quick setting bubble is collapsed).
  gfx::Size image_size = artwork_view_->GetImageBounds().size();
  int x = (kArtworkSize.width() - image_size.width()) / 2;
  int y = (kArtworkSize.height() - image_size.height()) / 2;
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(x, y, image_size.width(),
                                                image_size.height())),
                    kArtworkCornerRadius, kArtworkCornerRadius);
  return path;
}

}  // namespace ash
