// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/system/media/unified_media_controls_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr int kMediaControlsCornerRadius = 8;
constexpr int kMediaControlsViewPadding = 8;
constexpr int kMediaButtonsPadding = 8;
constexpr int kArtworkCornerRadius = 4;
constexpr int kTitleRowHeight = 20;
constexpr int kTrackTitleFontSizeIncrease = 1;

constexpr auto kTrackColumnInsets = gfx::Insets::TLBR(1, 8, 1, 8);
constexpr auto kMediaControlsViewInsets = gfx::Insets::TLBR(8, 8, 8, 12);

constexpr gfx::Size kEmptyArtworkIconSize = gfx::Size(20, 20);
constexpr gfx::Size kArtworkSize = gfx::Size(40, 40);

gfx::Size ScaleSizeToFitView(const gfx::Size& size,
                             const gfx::Size& view_size) {
  // If |size| is too small in either dimension or too big in both
  // dimensions, scale it appropriately.
  if ((size.width() > view_size.width() &&
       size.height() > view_size.height()) ||
      (size.width() < view_size.width() ||
       size.height() < view_size.height())) {
    const float scale =
        std::max(view_size.width() / static_cast<float>(size.width()),
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
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
    case MediaSessionAction::kRaise:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kPreviousSlide:
    case MediaSessionAction::kNextSlide:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return gfx::kNoneIcon;
}

SkColor GetBackgroundColor() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

}  // namespace

UnifiedMediaControlsView::MediaActionButton::MediaActionButton(
    UnifiedMediaControlsController* controller,
    MediaSessionAction action,
    int accessible_name_id)
    : IconButton(
          base::BindRepeating(
              // Handle dynamically-updated button tags without rebinding.
              [](UnifiedMediaControlsController* controller,
                 MediaActionButton* button) {
                controller->PerformAction(
                    media_message_center::GetActionFromButtonTag(*button));
              },
              controller,
              this),
          IconButton::Type::kMedium,
          &GetVectorIconForMediaAction(action),
          accessible_name_id),
      action_(action) {
  set_tag(static_cast<int>(action));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/false);
}

void UnifiedMediaControlsView::MediaActionButton::SetAction(
    MediaSessionAction action,
    const std::u16string& accessible_name) {
  action_ = action;
  set_tag(static_cast<int>(action));
  SetTooltipText(accessible_name);
  SetVectorIcon(GetVectorIconForMediaAction(action));
}

UnifiedMediaControlsView::UnifiedMediaControlsView(
    UnifiedMediaControlsController* controller)
    : views::Button(base::BindRepeating(
          [](UnifiedMediaControlsView* view) {
            if (!view->is_in_empty_state_)
              view->controller_->OnMediaControlsViewClicked();
          },
          this)),
      controller_(controller) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(),
                                                   kMediaControlsCornerRadius));
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMediaControlsViewInsets,
      kMediaControlsViewPadding));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

  auto artwork_view = std::make_unique<views::ImageView>();
  artwork_view->SetPreferredSize(kArtworkSize);
  artwork_view_ = AddChildView(std::move(artwork_view));
  artwork_view_->SetVisible(false);

  auto track_column = std::make_unique<views::View>();
  auto* track_column_layout =
      track_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kTrackColumnInsets));
  track_column_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

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

  title_label_ = title_row->AddChildView(std::make_unique<views::Label>());
  config_label(title_label_);
  title_label_->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(
          kTrackTitleFontSizeIncrease));

  drop_down_icon_ =
      title_row->AddChildView(std::make_unique<views::ImageView>());
  drop_down_icon_->SetPreferredSize(
      gfx::Size(kTitleRowHeight, kTitleRowHeight));

  title_row_layout->SetFlexForView(title_label_, 1);
  track_column->AddChildView(std::move(title_row));

  artist_label_ = track_column->AddChildView(std::make_unique<views::Label>());
  config_label(artist_label_);

  box_layout->SetFlexForView(AddChildView(std::move(track_column)), 1);

  auto button_row = std::make_unique<views::View>();
  button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kMediaButtonsPadding));

  button_row->AddChildView(std::make_unique<MediaActionButton>(
      controller_, MediaSessionAction::kPreviousTrack,
      IDS_ASH_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK));

  play_pause_button_ =
      button_row->AddChildView(std::make_unique<MediaActionButton>(
          controller_, MediaSessionAction::kPause,
          IDS_ASH_MEDIA_NOTIFICATION_ACTION_PAUSE));

  button_row->AddChildView(std::make_unique<MediaActionButton>(
      controller_, MediaSessionAction::kNextTrack,
      IDS_ASH_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK));

  button_row_ = AddChildView(std::move(button_row));

  // Focusable views must have an accessible name when shown/painted so that
  // the screen reader knows what to present to the user. SetTitle sets the
  // accessible name using a string which includes the title of the song being
  // played. That seems like the wrong string to use upon creation if nothing
  // is playing. Therefore setting the name to a string which lacks the "now
  // playing" information.
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_SUB_MENU_ACCESSIBLE_DESCRIPTION));
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
    absl::optional<gfx::ImageSkia> artwork) {
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

void UnifiedMediaControlsView::SetTitle(const std::u16string& title) {
  if (title_label_->GetText() == title)
    return;

  title_label_->SetText(title);
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_ACCESSIBLE_DESCRIPTION,
      title));
}

void UnifiedMediaControlsView::SetArtist(const std::u16string& artist) {
  artist_label_->SetText(artist);

  if (artist_label_->GetVisible() != artist.empty())
    return;

  artist_label_->SetVisible(!artist.empty());
  InvalidateLayout();
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

void UnifiedMediaControlsView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  background()->SetNativeControlColor(GetBackgroundColor());
  title_label_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  drop_down_icon_->SetImage(CreateVectorIcon(
      kUnifiedMenuMoreIcon,
      color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  artist_label_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
}

void UnifiedMediaControlsView::ShowEmptyState() {
  if (is_in_empty_state_)
    return;

  is_in_empty_state_ = true;

  title_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_GLOBAL_MEDIA_CONTROLS_NO_MEDIA_TEXT));
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  artist_label_->SetVisible(false);
  drop_down_icon_->SetVisible(false);

  for (views::View* button : button_row_->children())
    button->SetEnabled(false);
  InvalidateLayout();

  if (!artwork_view_->GetVisible())
    return;

  artwork_view_->SetBackground(views::CreateSolidBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::
              kControlBackgroundColorInactive)));
  artwork_view_->SetImageSize(kEmptyArtworkIconSize);
  artwork_view_->SetImage(CreateVectorIcon(
      kMusicNoteIcon, kEmptyArtworkIconSize.width(),
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorSecondary)));

  artwork_view_->SetClipPath(GetArtworkClipPath());
}

void UnifiedMediaControlsView::OnNewMediaSession() {
  if (!is_in_empty_state_)
    return;

  is_in_empty_state_ = false;
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  drop_down_icon_->SetVisible(true);

  for (views::View* button : button_row_->children())
    button->SetEnabled(true);
  InvalidateLayout();

  if (!artwork_view_->GetVisible())
    return;
  artwork_view_->SetBackground(nullptr);
}

SkPath UnifiedMediaControlsView::GetArtworkClipPath() {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(0, 0, kArtworkSize.width(),
                                                kArtworkSize.height())),
                    kArtworkCornerRadius, kArtworkCornerRadius);
  return path;
}

}  // namespace ash
