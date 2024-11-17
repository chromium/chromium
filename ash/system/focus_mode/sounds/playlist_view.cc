// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/playlist_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/playlist_image_button.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

constexpr int kSinglePlaylistViewWidth = 72;
constexpr int kSinglePlaylistViewSpacingBetweenChild = 10;
constexpr int kPlaylistTitleLineHeight = 16;
constexpr int kLoadingBackgroundCornerRadius = 16;
constexpr float kLoadingLayerOpacity = 0.06f;

}  // namespace

PlaylistView::PlaylistView(focus_mode_util::SoundType type,
                           TogglePlaylistCallback toggle_playlist_callback)
    : toggle_playlist_callback_(std::move(toggle_playlist_callback)) {
  playlist_data_.type = type;

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetBetweenChildSpacing(kSinglePlaylistViewSpacingBetweenChild);

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);

  playlist_image_button_ =
      AddChildView(std::make_unique<PlaylistImageButton>());
  playlist_image_button_->SetCallback(base::BindRepeating(
      &PlaylistView::OnPlaylistViewToggled, base::Unretained(this)));
  playlist_image_button_->GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  // Set the `playlist_image_button_` background color and the opacity for the
  // initial loading state.
  SetCanProcessEventsWithinSubtree(false);
  playlist_image_button_->SetBackground(
      views::CreateThemedRoundedRectBackground(cros_tokens::kCrosSysOnSurface,
                                               kLoadingBackgroundCornerRadius));
  playlist_image_button_->SetPaintToLayer();
  playlist_image_button_->layer()->SetFillsBoundsOpaquely(false);
  playlist_image_button_->layer()->SetOpacity(kLoadingLayerOpacity);

  title_label_ = AddChildView(std::make_unique<views::Label>());
  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  title_label_->SetMaximumWidthSingleLine(kSinglePlaylistViewWidth);
  title_label_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosAnnotation2));
  title_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  title_label_->SetLineHeight(kPlaylistTitleLineHeight);
  title_label_->GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  // Set the `title_label_` background color and the opacity for the initial
  // loading state.
  title_label_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysOnSurface, kLoadingBackgroundCornerRadius));
  title_label_->SetPaintToLayer();
  title_label_->layer()->SetFillsBoundsOpaquely(false);
  title_label_->layer()->SetOpacity(kLoadingLayerOpacity);
}

PlaylistView::~PlaylistView() = default;

void PlaylistView::UpdateContents(
    uint8_t position,
    const FocusModeSoundsController::Playlist& playlist) {
  CHECK_LT(position, kFocusModePlaylistViewsNum);
  playlist_data_.id = playlist.playlist_id;
  playlist_data_.title = playlist.title;
  playlist_data_.thumbnail = playlist.thumbnail;
  playlist_data_.list_position = position;

  // Remove the loading state styling.
  playlist_image_button_->SetBackground(nullptr);
  playlist_image_button_->DestroyLayer();
  title_label_->SetBackground(nullptr);
  title_label_->DestroyLayer();
  SetCanProcessEventsWithinSubtree(true);

  if (const auto text = base::UTF8ToUTF16(playlist_data_.title);
      !text.empty()) {
    title_label_->SetText(text);
    title_label_->SetTooltipText(text);
    title_label_->GetViewAccessibility().SetName(text);
    playlist_image_button_->SetTooltipText(text);
    playlist_image_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_PLAYLIST_ACCESSIBLE_NAME,
            text));
  }
  playlist_image_button_->UpdateContents(playlist_data_.thumbnail);
}

void PlaylistView::SetState(focus_mode_util::SoundState state) {
  playlist_data_.state = state;
  switch (state) {
    case focus_mode_util::SoundState::kNone:
      playlist_image_button_->SetIsSelected(false);
      playlist_image_button_->SetIsPlaying(false);
      break;
    case focus_mode_util::SoundState::kSelected:
    case focus_mode_util::SoundState::kPaused:
      playlist_image_button_->SetIsSelected(true);
      playlist_image_button_->SetIsPlaying(false);
      break;
    case focus_mode_util::SoundState::kPlaying:
      playlist_image_button_->SetIsSelected(true);
      playlist_image_button_->SetIsPlaying(true);
      break;
  }
}

void PlaylistView::OnPlaylistViewToggled() {
  CHECK(toggle_playlist_callback_);
  CHECK(!playlist_data_.empty());
  toggle_playlist_callback_.Run(playlist_data_);
}

BEGIN_METADATA(PlaylistView)
END_METADATA

}  // namespace ash
