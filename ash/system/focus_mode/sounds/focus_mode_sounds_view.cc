// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/playlist_image_button.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

constexpr auto kSoundContainerBottomInsets = 22;
constexpr auto kSoundTabSliderInsets = gfx::Insets::VH(16, 0);

constexpr int kPlaylistViewsNum = 4;
constexpr auto kPlaylistsContainerViewInsets = gfx::Insets::VH(0, 24);
constexpr int kSinglePlaylistViewWidth = 72;
constexpr int kSinglePlaylistViewSpacingBetweenChild = 10;
constexpr int kPlaylistTitleLineHeight = 10;

std::unique_ptr<views::View> CreateSpacerView() {
  auto spacer_view = std::make_unique<views::View>();
  spacer_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  return spacer_view;
}

std::unique_ptr<views::BoxLayoutView> CreatePlaylistsContainerView() {
  auto container_view = std::make_unique<views::BoxLayoutView>();
  container_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container_view->SetBorder(
      views::CreateEmptyBorder(kPlaylistsContainerViewInsets));

  return container_view;
}

//---------------------------------------------------------------------
// PlaylistView:

class PlaylistView : public views::BoxLayoutView {
  METADATA_HEADER(PlaylistView, views::BoxLayoutView)

 public:
  explicit PlaylistView(
      const FocusModeSoundsController::Playlist* playlist_data)
      : playlist_id_(playlist_data->playlist_id) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetBetweenChildSpacing(kSinglePlaylistViewSpacingBetweenChild);

    // TODO: Use a non-empty callback to create the `PlaylistImageButton`
    // after we know how to play the stream.
    thumbnail_view_ = AddChildView(std::make_unique<PlaylistImageButton>(
        playlist_data->thumbnail, views::Button::PressedCallback()));
    thumbnail_view_->SetTooltipText(base::UTF8ToUTF16(playlist_data->title));

    title_label_ = AddChildView(std::make_unique<views::Label>(
        base::UTF8ToUTF16(playlist_data->title)));
    title_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    title_label_->SetMaximumWidthSingleLine(kSinglePlaylistViewWidth);
    title_label_->SetFontList(
        ash::TypographyProvider::Get()->ResolveTypographyToken(
            ash::TypographyToken::kCrosLabel1));
    title_label_->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
    title_label_->SetLineHeight(kPlaylistTitleLineHeight);
    title_label_->SetTooltipText(title_label_->GetText());
  }

  std::string playlist_id() const { return playlist_id_; }

 private:
  std::string playlist_id_;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<PlaylistImageButton> thumbnail_view_ = nullptr;
};

BEGIN_METADATA(PlaylistView)
END_METADATA

}  // namespace

//---------------------------------------------------------------------
// FocusModeSoundsView:

FocusModeSoundsView::FocusModeSoundsView() {
  SetProperty(views::kMarginsKey, kDisconnectedContainerMargins);
  SetBorderInsets(gfx::Insets::TLBR(0, 0, kSoundContainerBottomInsets, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  CreateTabSliderButtons();

  soundscape_container_ = AddChildView(CreatePlaylistsContainerView());
  youtube_music_container_ = AddChildView(CreatePlaylistsContainerView());

  // We are currently defaulting to the premium playlists when opening a new
  // focus mode panel, and this can change based on future policies.
  youtube_music_button_->SetSelected(true);
  OnYoutubeMusicButtonToggled();

  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  sounds_controller->DownloadPlaylistsForType(
      /*is_soundscape_type=*/true,
      base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                     weak_factory_.GetWeakPtr()));
  sounds_controller->DownloadPlaylistsForType(
      /*is_soundscape_type=*/false,
      base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                     weak_factory_.GetWeakPtr()));
}

FocusModeSoundsView::~FocusModeSoundsView() = default;

void FocusModeSoundsView::UpdateSoundsView(bool is_soundscape_type) {
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  const auto& data = is_soundscape_type
                         ? sounds_controller->soundscape_playlists()
                         : sounds_controller->youtube_music_playlists();
  CHECK_EQ(static_cast<int>(data.size()), kPlaylistViewsNum);

  auto& playlist_views_list = is_soundscape_type
                                  ? soundscape_playlist_view_list_
                                  : youtube_music_playlist_view_list_;

  playlist_views_list.clear();
  for (size_t i = 0; i < kPlaylistViewsNum; ++i) {
    const auto& playlist_data = data.at(i);
    auto box_view =
        is_soundscape_type ? soundscape_container_ : youtube_music_container_;
    // Before appending a new `PlaylistView`, we add a spacer view to make the
    // spacing between each of the `PlaylistView` equal.
    if (i > 0) {
      auto* spacer_view = box_view->AddChildView(CreateSpacerView());
      box_view->SetFlexForView(spacer_view, 1);
    }
    playlist_views_list.push_back(box_view->AddChildView(
        std::make_unique<PlaylistView>(playlist_data.get())));
  }
}

void FocusModeSoundsView::CreateTabSliderButtons() {
  auto* tab_slider_box = AddChildView(std::make_unique<views::BoxLayoutView>());
  tab_slider_box->SetInsideBorderInsets(kSoundTabSliderInsets);
  tab_slider_box->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  auto* sound_tab_slider = tab_slider_box->AddChildView(
      std::make_unique<TabSlider>(/*max_tab_num=*/2));

  // TODO(b/326473049): Revisit the descriptions after getting the final
  // decision from UX/PM.
  soundscape_button_ = sound_tab_slider->AddButton<LabelSliderButton>(
      base::BindRepeating(&FocusModeSoundsView::OnSoundscapeButtonToggled,
                          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_SOUNDSCAPE_BUTTON));
  youtube_music_button_ = sound_tab_slider->AddButton<LabelSliderButton>(
      base::BindRepeating(&FocusModeSoundsView::OnYoutubeMusicButtonToggled,
                          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_BUTTON));
}

void FocusModeSoundsView::OnSoundscapeButtonToggled() {
  soundscape_container_->SetVisible(true);
  youtube_music_container_->SetVisible(false);
}

void FocusModeSoundsView::OnYoutubeMusicButtonToggled() {
  soundscape_container_->SetVisible(false);
  youtube_music_container_->SetVisible(true);
}

BEGIN_METADATA(FocusModeSoundsView)
END_METADATA

}  // namespace ash
