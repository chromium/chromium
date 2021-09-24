// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_advanced_settings_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Size kSettingsSize{256, 248};

constexpr gfx::RoundedCornersF kBorderRadius{10.f};

// All the options in the CaptureMode settings view.
enum CaptureSettingsOption {
  kAudioOff = 0,
  kAudioMicrophone,
  kDownloadsFolder,
  kCustomFolder,
};

}  // namespace

CaptureModeAdvancedSettingsView::CaptureModeAdvancedSettingsView()
    : audio_input_menu_group_(
          AddChildView(std::make_unique<CaptureModeMenuGroup>(
              this,
              kCaptureModeMicIcon,
              l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT)))),
      separator_(AddChildView(std::make_unique<views::Separator>())),
      save_to_menu_group_(AddChildView(std::make_unique<CaptureModeMenuGroup>(
          this,
          kCaptureModeFolderIcon,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO)))) {
  audio_input_menu_group_->AddOption(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_OFF),
      kAudioOff);
  audio_input_menu_group_->AddOption(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_MICROPHONE),
      kAudioMicrophone);
  save_to_menu_group_->AddOption(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_DOWNLOADS),
      kDownloadsFolder);
  save_to_menu_group_->AddMenuItem(
      base::BindRepeating(&CaptureModeAdvancedSettingsView::HandleMenuClick,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_SELECT_FOLDER));

  auto* color_provider = AshColorProvider::Get();

  const SkColor separator_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  separator_->SetColor(separator_color);

  SetPaintToLayer();
  SetBackground(views::CreateSolidBackground(color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80)));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

CaptureModeAdvancedSettingsView::~CaptureModeAdvancedSettingsView() = default;

gfx::Rect CaptureModeAdvancedSettingsView::GetBounds(
    CaptureModeBarView* capture_mode_bar_view) {
  DCHECK(capture_mode_bar_view);

  return gfx::Rect(
      capture_mode_bar_view->settings_button()->GetBoundsInScreen().right() -
          kSettingsSize.width(),
      capture_mode_bar_view->GetBoundsInScreen().y() -
          capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu -
          kSettingsSize.height(),
      kSettingsSize.width(), kSettingsSize.height());
}

void CaptureModeAdvancedSettingsView::OnOptionSelected(int option_id) const {
  switch (option_id) {
    case kAudioOff:
      CaptureModeController::Get()->EnableAudioRecording(false);
      break;
    case kAudioMicrophone:
      CaptureModeController::Get()->EnableAudioRecording(true);
      break;
    case kDownloadsFolder:
    case kCustomFolder:
      // TODO(conniekxu|afakhry): Handle |kDownloadsFolder| and |kCustomFolder|
      // options in the following up CLs. For now we only support
      // |kDownloadsFolder| for |save_to_menu_group_|, that's why we don't need
      // to handle it explicitly here.
      break;
    default:
      return;
  }
}

bool CaptureModeAdvancedSettingsView::IsOptionChecked(int option_id) const {
  switch (option_id) {
    case kAudioOff:
      return !CaptureModeController::Get()->enable_audio_recording();
    case kAudioMicrophone:
      return CaptureModeController::Get()->enable_audio_recording();
    // TODO(conniekxu|afakhry): Handle |kDownloadsFolder| and |kCustomFolder|
    // options in the following up CLs. For now we only support
    // |kDownloadsFolder|, hence we return true/false directly here.
    case kDownloadsFolder:
      return true;
    case kCustomFolder:
      return false;
    default:
      return false;
  }
}

views::View* CaptureModeAdvancedSettingsView::GetMicrophoneOptionForTesting() {
  return audio_input_menu_group_->GetOptionForTesting(  // IN-TEST
      kAudioMicrophone);                                // IN-TEST
}

views::View* CaptureModeAdvancedSettingsView::GetOffOptionForTesting() {
  return audio_input_menu_group_->GetOptionForTesting(kAudioOff);  // IN-TEST
}

void CaptureModeAdvancedSettingsView::HandleMenuClick() {}

BEGIN_METADATA(CaptureModeAdvancedSettingsView, views::View)
END_METADATA

}  // namespace ash