// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_advanced_settings_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/files/file_path.h"
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

// Returns the bounds of the settings widget in screen coordinates relative to
// the bounds of the |capture_mode_bar_view| based on its given preferred
// |settings_view_size|.
gfx::Rect GetWidgetBounds(CaptureModeBarView* capture_mode_bar_view,
                          const gfx::Size& settings_view_size) {
  DCHECK(capture_mode_bar_view);

  return gfx::Rect(
      capture_mode_bar_view->settings_button()->GetBoundsInScreen().right() -
          kSettingsSize.width(),
      capture_mode_bar_view->GetBoundsInScreen().y() -
          capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu -
          settings_view_size.height(),
      kSettingsSize.width(), settings_view_size.height());
}

CaptureModeController::CaptureFolder GetCurrentCaptureFolder() {
  return CaptureModeController::Get()->GetCurrentCaptureFolder();
}

}  // namespace

CaptureModeAdvancedSettingsView::CaptureModeAdvancedSettingsView(
    CaptureModeSession* session,
    bool is_in_projector_mode)
    : capture_mode_session_(session),
      audio_input_menu_group_(
          AddChildView(std::make_unique<CaptureModeMenuGroup>(
              this,
              kCaptureModeMicIcon,
              l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT)))) {
  if (!is_in_projector_mode) {
    audio_input_menu_group_->AddOption(
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_OFF),
        kAudioOff);
  }
  audio_input_menu_group_->AddOption(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_MICROPHONE),
      kAudioMicrophone);

  auto* color_provider = AshColorProvider::Get();
  if (!is_in_projector_mode) {
    separator_ = AddChildView(std::make_unique<views::Separator>());

    save_to_menu_group_ = AddChildView(std::make_unique<CaptureModeMenuGroup>(
        this, kCaptureModeFolderIcon,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO)));
    save_to_menu_group_->AddOption(
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_DOWNLOADS),
        kDownloadsFolder);
    save_to_menu_group_->AddMenuItem(
        base::BindRepeating(
            &CaptureModeAdvancedSettingsView::OnSelectFolderMenuItemPressed,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(
            IDS_ASH_SCREEN_CAPTURE_SAVE_TO_SELECT_FOLDER));

    const SkColor separator_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kSeparatorColor);
    separator_->SetColor(separator_color);
  }

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
    CaptureModeBarView* capture_mode_bar_view,
    CaptureModeAdvancedSettingsView* content_view) {
  DCHECK(capture_mode_bar_view);

  const gfx::Size settings_size =
      content_view ? content_view->GetPreferredSize() : kSettingsSize;
  return GetWidgetBounds(capture_mode_bar_view, settings_size);
}

void CaptureModeAdvancedSettingsView::OnCaptureFolderMayHaveChanged() {
  if (!save_to_menu_group_)
    return;
  auto* controller = CaptureModeController::Get();
  const auto custom_path = controller->GetCustomCaptureFolder();
  if (custom_path.empty()) {
    is_custom_folder_available_.reset();
    save_to_menu_group_->RemoveOptionIfAny(kCustomFolder);
    save_to_menu_group_->RefreshOptionsSelections();
    return;
  }

  std::u16string folder_name = custom_path.BaseName().AsUTF16Unsafe();
  // We explicitly name the folders of Google Drive and Play files, since those
  // folders internally may have user-unfriendly names.
  if (controller->IsRootDriveFsPath(custom_path)) {
    folder_name =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_GOOGLE_DRIVE);
  } else if (controller->IsAndroidFilesPath(custom_path)) {
    folder_name =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_ANDROID_FILES);
  }

  save_to_menu_group_->AddOrUpdateExistingOption(folder_name, kCustomFolder);

  controller->CheckFolderAvailability(
      custom_path,
      base::BindOnce(
          &CaptureModeAdvancedSettingsView::OnCustomFolderAvailabilityChecked,
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeAdvancedSettingsView::OnDefaultCaptureFolderSelectionChanged() {
  if (save_to_menu_group_)
    save_to_menu_group_->RefreshOptionsSelections();
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureModeAdvancedSettingsView::GetHighlightableItems() {
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
      highlightable_items;
  DCHECK(audio_input_menu_group_);
  audio_input_menu_group_->AppendHighlightableItems(highlightable_items);
  if (save_to_menu_group_)
    save_to_menu_group_->AppendHighlightableItems(highlightable_items);
  return highlightable_items;
}

void CaptureModeAdvancedSettingsView::OnOptionSelected(int option_id) const {
  auto* controller = CaptureModeController::Get();
  switch (option_id) {
    case kAudioOff:
      controller->EnableAudioRecording(false);
      break;
    case kAudioMicrophone:
      controller->EnableAudioRecording(true);
      break;
    case kDownloadsFolder:
      controller->SetUsesDefaultCaptureFolder(true);
      RecordSwitchToDefaultFolderReason(
          CaptureModeSwitchToDefaultReason::kUserSelectedFromSettingsMenu);
      break;
    case kCustomFolder:
      controller->SetUsesDefaultCaptureFolder(false);
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
    case kDownloadsFolder:
      return GetCurrentCaptureFolder().is_default_downloads_folder ||
             !is_custom_folder_available_.value_or(false);
    case kCustomFolder:
      return !GetCurrentCaptureFolder().is_default_downloads_folder &&
             is_custom_folder_available_.value_or(false);
    default:
      return false;
  }
}

bool CaptureModeAdvancedSettingsView::IsOptionEnabled(int option_id) const {
  switch (option_id) {
    case kAudioOff:
      return !capture_mode_session_->is_in_projector_mode();
    case kCustomFolder:
      return is_custom_folder_available_.value_or(false);
    case kAudioMicrophone:
    case kDownloadsFolder:
    default:
      return true;
  }
}

views::View* CaptureModeAdvancedSettingsView::GetMicrophoneOptionForTesting() {
  return audio_input_menu_group_->GetOptionForTesting(  // IN-TEST
      kAudioMicrophone);                                // IN-TEST
}

views::View* CaptureModeAdvancedSettingsView::GetOffOptionForTesting() {
  return audio_input_menu_group_->GetOptionForTesting(kAudioOff);  // IN-TEST
}

void CaptureModeAdvancedSettingsView::OnSelectFolderMenuItemPressed() {
  capture_mode_session_->OpenFolderSelectionDialog();
}

void CaptureModeAdvancedSettingsView::OnCustomFolderAvailabilityChecked(
    bool available) {
  DCHECK(save_to_menu_group_);
  is_custom_folder_available_ = available;
  save_to_menu_group_->RefreshOptionsSelections();
  if (!is_custom_folder_available_.value_or(false)) {
    RecordSwitchToDefaultFolderReason(
        CaptureModeSwitchToDefaultReason::kFolderUnavailable);
  }
  if (on_settings_menu_refreshed_callback_for_test_)
    std::move(on_settings_menu_refreshed_callback_for_test_).Run();
}

BEGIN_METADATA(CaptureModeAdvancedSettingsView, views::View)
END_METADATA

}  // namespace ash
