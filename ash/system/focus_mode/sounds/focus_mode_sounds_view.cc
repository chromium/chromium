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
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/playlist_view.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/grit/ash_internal_scaled_resources.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

constexpr auto kSoundViewBottomPadding = 22;
constexpr auto kSoundTabSliderInsets = gfx::Insets::VH(16, 0);

constexpr int kNonPremiumChildViewsSpacing = 16;
constexpr int kNonPremiumLabelViewMaxWidth = 288;

constexpr float kOfflineStateOpacity = 0.38f;
constexpr auto kLabelPadding = gfx::Insets::VH(0, 40);

std::optional<int> GetYouTubeMusicIconResourceId() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_YOUTUBE_MUSIC_ICON;
#else
  return std::nullopt;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

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
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

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

  // Add the YouTube Music icon for the `learn_more_button` if it's chrome
  // branded.
  const auto& resource_id = GetYouTubeMusicIconResourceId();
  if (resource_id.has_value()) {
    const gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            resource_id.value());

    CHECK(image);
    learn_more_button->SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                                     ui::ImageModel::FromImageSkia(*image));
  }

  return box_view;
}

std::unique_ptr<views::Label> CreateOfflineLabel(const int message_id) {
  auto label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(message_id));
  label->SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
      ash::TypographyToken::kCrosBody2));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  return label;
}

std::unique_ptr<views::BoxLayoutView> CreateOfflineStateView() {
  auto box_view = std::make_unique<views::BoxLayoutView>();
  box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_view->SetBorder(views::CreateEmptyBorder(kLabelPadding));
  box_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_view->AddChildView(CreateOfflineLabel(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OFFLINE_LABEL_ONE));
  box_view->AddChildView(CreateOfflineLabel(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OFFLINE_LABEL_TWO));
  return box_view;
}

}  // namespace

//---------------------------------------------------------------------
// FocusModeSoundsView:

FocusModeSoundsView::FocusModeSoundsView(
    const base::flat_set<focus_mode_util::SoundType>& sound_sections,
    bool is_network_connected) {
  SetProperty(views::kMarginsKey, kDisconnectedContainerMargins);
  SetBorderInsets(gfx::Insets::TLBR(0, 0, kSoundViewBottomPadding, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (sound_sections.empty()) {
    SetVisible(false);
    return;
  }
  CreateTabSliderButtons(sound_sections, is_network_connected);

  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();

  const bool should_show_soundscapes =
      focus_mode_util::SoundType::kSoundscape ==
          sounds_controller->sound_type() ||
      !base::Contains(sound_sections,
                      focus_mode_util::SoundType::kYouTubeMusic);
  if (should_show_soundscapes) {
    soundscape_button_->SetSelected(true);
  } else {
    youtube_music_button_->SetSelected(true);
  }

  if (is_network_connected) {
    CreatesSoundSectionViews(sound_sections);

    if (soundscape_container_) {
      // Start downloading playlists for Soundscape.
      sounds_controller->DownloadPlaylistsForType(
          /*is_soundscape_type=*/true,
          base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                         weak_factory_.GetWeakPtr()));
    }

    if (youtube_music_container_) {
      // Set failure callback and start downloading playlists for YouTube Music.
      sounds_controller->SetYouTubeMusicFailureCallback(base::BindRepeating(
          &FocusModeSoundsView::ToggleYouTubeMusicAlternateView,
          weak_factory_.GetWeakPtr(), /*show=*/true));
      sounds_controller->DownloadPlaylistsForType(
          /*is_soundscape_type=*/false,
          base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                         weak_factory_.GetWeakPtr()));
    }

    if (should_show_soundscapes) {
      OnSoundscapeButtonToggled();
    } else {
      OnYouTubeMusicButtonToggled();
    }
  } else {
    AddChildView(CreateOfflineStateView());
  }

  sounds_controller->AddObserver(this);
}

FocusModeSoundsView::~FocusModeSoundsView() {
  FocusModeController::Get()->focus_mode_sounds_controller()->RemoveObserver(
      this);
}

void FocusModeSoundsView::OnSelectedPlaylistChanged() {
  const auto& selected_playlist = FocusModeController::Get()
                                      ->focus_mode_sounds_controller()
                                      ->selected_playlist();
  UpdateStateForSelectedPlaylist(selected_playlist);
}

void FocusModeSoundsView::OnPlaylistStateChanged() {
  const auto& selected_playlist = FocusModeController::Get()
                                      ->focus_mode_sounds_controller()
                                      ->selected_playlist();
  if (selected_playlist.empty()) {
    UpdateStateForSelectedPlaylist(selected_playlist);
    return;
  }

  switch (selected_playlist.type) {
    case focus_mode_util::SoundType::kSoundscape:
      if (soundscape_container_) {
        soundscape_container_->UpdateSelectedPlaylistForNewState(
            selected_playlist.state);
      }
      break;
    case focus_mode_util::SoundType::kYouTubeMusic:
      if (youtube_music_container_) {
        youtube_music_container_->UpdateSelectedPlaylistForNewState(
            selected_playlist.state);
      }
      break;
    case focus_mode_util::SoundType::kNone:
      NOTREACHED_NORETURN();
  }
}

void FocusModeSoundsView::UpdateSoundsView(bool is_soundscape_type) {
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  if (is_soundscape_type) {
    if (!soundscape_container_) {
      return;
    }
    const auto& playlists = sounds_controller->soundscape_playlists();
    if (playlists.empty()) {
      return;
    }
    soundscape_container_->UpdateContents(playlists);
  } else {
    if (!youtube_music_container_) {
      return;
    }
    const auto& playlists = sounds_controller->youtube_music_playlists();
    if (playlists.empty()) {
      return;
    }
    youtube_music_container_->UpdateContents(playlists);
  }
}

void FocusModeSoundsView::UpdateStateForSelectedPlaylist(
    const focus_mode_util::SelectedPlaylist& selected_playlist) {
  if (soundscape_container_) {
    soundscape_container_->UpdateStateForSelectedPlaylist(selected_playlist);
  }
  if (youtube_music_container_) {
    youtube_music_container_->UpdateStateForSelectedPlaylist(selected_playlist);
  }
}

void FocusModeSoundsView::CreateTabSliderButtons(
    const base::flat_set<focus_mode_util::SoundType>& sections,
    bool is_network_connected) {
  CHECK(!sections.empty());
  auto* tab_slider_box = AddChildView(std::make_unique<views::BoxLayoutView>());
  tab_slider_box->SetInsideBorderInsets(kSoundTabSliderInsets);
  tab_slider_box->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  auto* sound_tab_slider = tab_slider_box->AddChildView(
      std::make_unique<TabSlider>(/*max_tab_num=*/2));

  if (base::Contains(sections, focus_mode_util::SoundType::kSoundscape)) {
    soundscape_button_ = sound_tab_slider->AddButton<LabelSliderButton>(
        base::BindRepeating(&FocusModeSoundsView::OnSoundscapeButtonToggled,
                            weak_factory_.GetWeakPtr()),
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_SOUNDSCAPE_BUTTON));
  }
  if (base::Contains(sections, focus_mode_util::SoundType::kYouTubeMusic)) {
    youtube_music_button_ = sound_tab_slider->AddButton<LabelSliderButton>(
        base::BindRepeating(&FocusModeSoundsView::OnYouTubeMusicButtonToggled,
                            weak_factory_.GetWeakPtr()),
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_BUTTON));
  }

  if (!is_network_connected) {
    sound_tab_slider->layer()->SetOpacity(kOfflineStateOpacity);
    sound_tab_slider->SetEnabled(false);
  }
}

void FocusModeSoundsView::CreatesSoundSectionViews(
    const base::flat_set<focus_mode_util::SoundType>& sound_sections) {
  if (base::Contains(sound_sections, focus_mode_util::SoundType::kSoundscape)) {
    soundscape_container_ = AddChildView(std::make_unique<SoundSectionView>(
        focus_mode_util::SoundType::kSoundscape));
  }

  if (base::Contains(sound_sections,
                     focus_mode_util::SoundType::kYouTubeMusic)) {
    youtube_music_container_ = AddChildView(std::make_unique<SoundSectionView>(
        focus_mode_util::SoundType::kYouTubeMusic));
    youtube_music_container_->SetAlternateView(CreateNonPremiumView());
    ToggleYouTubeMusicAlternateView(/*show=*/false);
  }
}

void FocusModeSoundsView::ToggleYouTubeMusicAlternateView(bool show) {
  CHECK(youtube_music_container_);
  youtube_music_container_->ShowAlternateView(show);
}

void FocusModeSoundsView::OnSoundscapeButtonToggled() {
  if (soundscape_container_) {
    soundscape_container_->SetVisible(true);
  }
  if (youtube_music_container_) {
    youtube_music_container_->SetVisible(false);
  }
}

void FocusModeSoundsView::OnYouTubeMusicButtonToggled() {
  if (soundscape_container_) {
    soundscape_container_->SetVisible(false);
  }
  if (youtube_music_container_) {
    youtube_music_container_->SetVisible(true);
  }
}

BEGIN_METADATA(FocusModeSoundsView)
END_METADATA

}  // namespace ash
