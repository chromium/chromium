// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"

#include <memory>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

constexpr auto kSoundViewBottomPadding = 22;
constexpr auto kSoundTabSliderInsets = gfx::Insets::VH(16, 0);

constexpr int kNonPremiumChildViewsSpacing = 16;
constexpr int kNonPremiumLabelViewMaxWidth = 288;

std::unique_ptr<views::BoxLayoutView> CreateNonPremiumView() {
  auto box_view = std::make_unique<views::BoxLayoutView>();
  box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_view->SetBetweenChildSpacing(kNonPremiumChildViewsSpacing);

  auto* label = box_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_NON_PREMIUM_LABEL)));
  label->SetMultiLine(true);
  // For the label view with multiple lines, we need to set the max width for
  // it to calculate the total height of multiple lines.
  label->SetMaximumWidth(kNonPremiumLabelViewMaxWidth);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody2));

  auto* learn_more_button = box_view->AddChildView(std::make_unique<PillButton>(
      views::Button::PressedCallback(base::BindRepeating([]() {
        Shell::Get()
            ->system_tray_model()
            ->client()
            ->ShowYouTubeMusicPremiumPage();
      })),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_LEARN_MORE_BUTTON),
      PillButton::Type::kDefaultElevatedWithIconLeading));

  const gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_DEFAULT_FAVICON_32);
  DCHECK(image);

  // TODO(b/332922522): After adding the YTM icon, we can remove the resize
  // step below and its include file.
  gfx::ImageSkia resized_image = *image;
  resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      *image, skia::ImageOperations::RESIZE_BEST, gfx::Size(20, 20));

  learn_more_button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(resized_image));

  return box_view;
}

}  // namespace

//---------------------------------------------------------------------
// FocusModeSoundsView:

FocusModeSoundsView::FocusModeSoundsView() {
  SetProperty(views::kMarginsKey, kDisconnectedContainerMargins);
  SetBorderInsets(gfx::Insets::TLBR(0, 0, kSoundViewBottomPadding, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  CreateTabSliderButtons();

  soundscape_container_ = AddChildView(std::make_unique<SoundSectionView>());
  youtube_music_container_ = AddChildView(std::make_unique<SoundSectionView>());

  // TODO: Assume that the user doesn't have a premium account currently. Will
  // add a condition here when we finish the API implementation.
  youtube_music_container_->SetAlternateView(CreateNonPremiumView());
  youtube_music_container_->ShowAlternateView(true);

  // We are currently defaulting to the premium playlists when opening a new
  // focus mode panel, and this can change based on future policies.
  youtube_music_button_->SetSelected(true);
  OnYouTubeMusicButtonToggled();

  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  sounds_controller->DownloadPlaylistsForType(
      /*is_soundscape_type=*/true,
      base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                     weak_factory_.GetWeakPtr()));
}

FocusModeSoundsView::~FocusModeSoundsView() = default;

void FocusModeSoundsView::UpdateSoundsView(bool is_soundscape_type) {
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  if (is_soundscape_type) {
    soundscape_container_->UpdateContents(
        sounds_controller->soundscape_playlists());
  } else {
    youtube_music_container_->UpdateContents(
        sounds_controller->youtube_music_playlists());
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
      base::BindRepeating(&FocusModeSoundsView::OnYouTubeMusicButtonToggled,
                          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_BUTTON));
}

void FocusModeSoundsView::OnSoundscapeButtonToggled() {
  soundscape_container_->SetVisible(true);
  youtube_music_container_->SetVisible(false);
}

void FocusModeSoundsView::OnYouTubeMusicButtonToggled() {
  soundscape_container_->SetVisible(false);
  youtube_music_container_->SetVisible(true);
}

BEGIN_METADATA(FocusModeSoundsView)
END_METADATA

}  // namespace ash
