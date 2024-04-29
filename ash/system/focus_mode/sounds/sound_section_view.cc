// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/sound_section_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/sounds/playlist_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

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

//---------------------------------------------------------------------
// PlaylistView:

class PlaylistView : public views::BoxLayoutView {
  METADATA_HEADER(PlaylistView, views::BoxLayoutView)

 public:
  PlaylistView() {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetBetweenChildSpacing(kSinglePlaylistViewSpacingBetweenChild);

    thumbnail_view_ = AddChildView(std::make_unique<PlaylistImageButton>());

    title_label_ = AddChildView(std::make_unique<views::Label>());
    title_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    title_label_->SetMaximumWidthSingleLine(kSinglePlaylistViewWidth);
    title_label_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosLabel1));
    title_label_->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
    title_label_->SetLineHeight(kPlaylistTitleLineHeight);
  }

  const std::string& playlist_id() const { return playlist_id_; }

  void UpdateContents(
      const FocusModeSoundsController::Playlist* playlist_data) {
    playlist_id_ = playlist_data->playlist_id;
    const auto text = base::UTF8ToUTF16(playlist_data->title);
    title_label_->SetText(text);
    title_label_->SetTooltipText(text);
    thumbnail_view_->SetTooltipText(text);
    // TODO: Use a non-empty callback for the `PlaylistImageButton`.
    thumbnail_view_->UpdateContents(playlist_data->thumbnail,
                                    views::Button::PressedCallback());
  }

 private:
  std::string playlist_id_;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<PlaylistImageButton> thumbnail_view_ = nullptr;
};

BEGIN_METADATA(PlaylistView)
END_METADATA

}  // namespace

//---------------------------------------------------------------------
// SoundSectionView:

SoundSectionView::SoundSectionView() {
  SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetDefault(views::kFlexBehaviorKey,
             views::FlexSpecification(
                 views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                 views::MaximumFlexSizeRule::kUnbounded));

  CreatePlaylistViewsContainer();
}

SoundSectionView::~SoundSectionView() = default;

void SoundSectionView::UpdateContents(
    const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
        data) {
  CHECK_EQ(static_cast<int>(data.size()), kPlaylistViewsNum);
  CHECK_EQ(static_cast<int>(playlist_view_list_.size()), kPlaylistViewsNum);

  for (size_t i = 0; i < kPlaylistViewsNum; ++i) {
    const auto& playlist_data = data.at(i);
    auto* playlist_view =
        views::AsViewClass<PlaylistView>(playlist_view_list_.at(i));
    playlist_view->UpdateContents(playlist_data.get());
  }
}

void SoundSectionView::ShowAlternateView(bool show_alternate_view) {
  CHECK(alternate_view_);

  playlist_views_container_->SetVisible(!show_alternate_view);
  alternate_view_->SetVisible(show_alternate_view);
}

void SoundSectionView::SetAlternateView(
    std::unique_ptr<views::BoxLayoutView> alternate_view) {
  CHECK(alternate_view);
  alternate_view_ = AddChildView(std::move(alternate_view));
}

void SoundSectionView::CreatePlaylistViewsContainer() {
  playlist_views_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  playlist_views_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  playlist_views_container_->SetBorder(
      views::CreateEmptyBorder(kPlaylistsContainerViewInsets));

  for (size_t i = 0; i < kPlaylistViewsNum; ++i) {
    // Before appending a new `PlaylistView`, we add a spacer view to make the
    // spacing between each of the `PlaylistView` equal.
    if (i > 0) {
      auto* spacer_view =
          playlist_views_container_->AddChildView(CreateSpacerView());
      playlist_views_container_->SetFlexForView(spacer_view, 1);
    }
    playlist_view_list_.push_back(playlist_views_container_->AddChildView(
        std::make_unique<PlaylistView>()));
  }
}

BEGIN_METADATA(SoundSectionView)
END_METADATA

}  // namespace ash
