// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/sound_section_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"
#include "ash/system/focus_mode/sounds/playlist_view.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr auto kPlaylistsContainerViewInsets = gfx::Insets::VH(0, 24);

std::unique_ptr<views::View> CreateSpacerView() {
  auto spacer_view = std::make_unique<views::View>();
  spacer_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  return spacer_view;
}

}  // namespace

//---------------------------------------------------------------------
// SoundSectionView:

SoundSectionView::SoundSectionView(focus_mode_util::SoundType type)
    : type_(type) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  CreatePlaylistViewsContainer(type);
}

SoundSectionView::~SoundSectionView() = default;

void SoundSectionView::UpdateContents(
    const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
        data) {
  CHECK_EQ(data.size(), kFocusModePlaylistViewsNum);
  CHECK_EQ(playlist_view_list_.size(), kFocusModePlaylistViewsNum);

  for (size_t i = 0; i < kFocusModePlaylistViewsNum; ++i) {
    const auto& playlist_data = data.at(i);
    auto* playlist_view = playlist_view_list_.at(i);
    playlist_view->UpdateContents(i, *playlist_data);
  }

  UpdateStateForSelectedPlaylist(FocusModeController::Get()
                                     ->focus_mode_sounds_controller()
                                     ->selected_playlist());
}

void SoundSectionView::ShowAlternateView(bool show_alternate_view) {
  CHECK(alternate_view_);

  playlist_views_container_->SetVisible(!show_alternate_view);
  alternate_view_->SetVisible(show_alternate_view);
}

void SoundSectionView::SetAlternateView(
    std::unique_ptr<views::BoxLayoutView> alternate_view) {
  CHECK(alternate_view);
  if (alternate_view_.get()) {
    RemoveChildViewT(std::exchange(alternate_view_, nullptr));
  }

  alternate_view_ = AddChildView(std::move(alternate_view));
}

bool SoundSectionView::IsAlternateViewVisible() const {
  return alternate_view_ && alternate_view_->GetVisible();
}

void SoundSectionView::UpdateStateForSelectedPlaylist(
    const focus_mode_util::SelectedPlaylist& selected_playlist) {
  for (auto* playlist_view : playlist_view_list_) {
    if (!selected_playlist.empty() && selected_playlist.type == type_ &&
        selected_playlist.id == playlist_view->playlist_data().id) {
      playlist_view->SetState(selected_playlist.state);
    } else {
      playlist_view->SetState(focus_mode_util::SoundState::kNone);
    }
  }
}

void SoundSectionView::UpdateSelectedPlaylistForNewState(
    focus_mode_util::SoundState new_state) {
  for (auto* playlist_view : playlist_view_list_) {
    if (playlist_view->playlist_data().state !=
        focus_mode_util::SoundState::kNone) {
      playlist_view->SetState(new_state);
      return;
    }
  }
}

void SoundSectionView::CreatePlaylistViewsContainer(
    focus_mode_util::SoundType type) {
  playlist_views_container_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  playlist_views_container_->SetMainAxisAlignment(
      views::LayoutAlignment::kCenter);
  playlist_views_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kCenter);
  playlist_views_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  playlist_views_container_->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kPreferred));
  playlist_views_container_->SetBorder(
      views::CreateEmptyBorder(kPlaylistsContainerViewInsets));

  for (size_t i = 0; i < kFocusModePlaylistViewsNum; ++i) {
    // Before appending a new `PlaylistView`, we add a spacer view to make the
    // spacing between each of the `PlaylistView` equal.
    if (i > 0) {
      auto* spacer_view =
          playlist_views_container_->AddChildView(CreateSpacerView());
      spacer_view->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)
              .WithWeight(1));
    }

    // `FocusModeSoundsController` is owned by `FocusModeController` which
    // outlives `PlaylistView`, because `FocusModeController` is destroyed after
    // the call of `CloseAllRootWindowChildWindows` in the dtor of `Shell`.
    playlist_view_list_.push_back(
        playlist_views_container_->AddChildView(std::make_unique<PlaylistView>(
            type,
            base::BindRepeating(
                &FocusModeSoundsController::TogglePlaylist,
                base::Unretained(FocusModeController::Get()
                                     ->focus_mode_sounds_controller())))));
  }
}

BEGIN_METADATA(SoundSectionView)
END_METADATA

}  // namespace ash
