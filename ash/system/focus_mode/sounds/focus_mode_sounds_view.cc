// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

constexpr auto kSoundContainerBottomInsets = 22;
constexpr auto kSoundTabSliderInsets = gfx::Insets::VH(16, 0);

}  // namespace

//---------------------------------------------------------------------
// FocusModeSoundsView:

FocusModeSoundsView::FocusModeSoundsView() {
  SetProperty(views::kMarginsKey, kDisconnectedContainerMargins);
  SetBorderInsets(gfx::Insets::TLBR(0, 0, kSoundContainerBottomInsets, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  CreateTabSliderButtons();
}

FocusModeSoundsView::~FocusModeSoundsView() = default;

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
  youtube_music_button_->SetSelected(true);
  OnYoutubeMusicButtonToggled();
}

void FocusModeSoundsView::OnSoundscapeButtonToggled() {}

void FocusModeSoundsView::OnYoutubeMusicButtonToggled() {}

BEGIN_METADATA(FocusModeSoundsView)
END_METADATA

}  // namespace ash
