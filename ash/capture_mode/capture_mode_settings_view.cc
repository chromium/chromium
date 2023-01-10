// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_settings_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "capture_mode_menu_toggle_button.h"
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

CaptureModeSettingsView::CaptureModeSettingsView(CaptureModeSession* session,
                                                 bool is_in_projector_mode)
    : capture_mode_session_(session) {
  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress()) {
    const bool audio_capture_managed_by_policy =
        controller->IsAudioCaptureDisabledByPolicy();

    DCHECK(!audio_capture_managed_by_policy || !is_in_projector_mode)
        << "A projector session should not be allowed to begin if audio "
           "recording is diabled by policy.";

    audio_input_menu_group_ =
        AddChildView(std::make_unique<CaptureModeMenuGroup>(
            this, kCaptureModeMicIcon,
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT),
            audio_capture_managed_by_policy));

    if (!is_in_projector_mode) {
      audio_input_menu_group_->AddOption(
          /*option_icon=*/nullptr,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_OFF),
          kAudioOff);
    }

    if (!audio_capture_managed_by_policy) {
      audio_input_menu_group_->AddOption(
          /*option_icon=*/nullptr,
          l10n_util::GetStringUTF16(
              IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_MICROPHONE),
          kAudioMicrophone);
    }

    separator_1_ = AddChildView(std::make_unique<views::Separator>());
    separator_1_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    auto* camera_controller = controller->camera_controller();
    const bool camera_managed_by_policy =
        camera_controller->IsCameraDisabledByPolicy();
    // Even if the camera feature is managed by policy, we still want to observe
    // the camera controller, since we need to be notified with camera additions
    // and removals, which affect the visibility of the `camera_menu_group_`.
    camera_controller->AddObserver(this);
    camera_menu_group_ = AddChildView(std::make_unique<CaptureModeMenuGroup>(
        this, kCaptureModeCameraIcon,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_CAMERA),
        camera_managed_by_policy));

    AddCameraOptions(camera_controller->available_cameras(),
                     camera_managed_by_policy);
  }

  if (features::AreCaptureModeDemoToolsEnabled()) {
    separator_2_ = AddChildView(std::make_unique<views::Separator>());
    separator_2_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    demo_tools_menu_toggle_button_ =
        AddChildView(std::make_unique<CaptureModeMenuToggleButton>(
            kCaptureModeDemoToolsSettingsMenuEntryPointIcon,
            l10n_util::GetStringUTF16(
                IDS_ASH_SCREEN_CAPTURE_DEMO_TOOLS_SHOW_CLICKS_AND_KEYS),
            CaptureModeController::Get()->enable_demo_tools(),
            base::BindRepeating(
                &CaptureModeSettingsView::OnDemoToolsButtonToggled,
                base::Unretained(this))));
  }

  if (!is_in_projector_mode) {
    separator_3_ = AddChildView(std::make_unique<views::Separator>());
    separator_3_->SetColorId(ui::kColorAshSystemUIMenuSeparator);

    save_to_menu_group_ = AddChildView(std::make_unique<CaptureModeMenuGroup>(
        this, kCaptureModeFolderIcon,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO)));
    save_to_menu_group_->AddOption(
        /*option_icon=*/nullptr,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_DOWNLOADS),
        kDownloadsFolder);
    save_to_menu_group_->AddMenuItem(
        base::BindRepeating(
            &CaptureModeSettingsView::OnSelectFolderMenuItemPressed,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(
            IDS_ASH_SCREEN_CAPTURE_SAVE_TO_SELECT_FOLDER));
  }

  SetPaintToLayer();
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

CaptureModeSettingsView::~CaptureModeSettingsView() {
  CaptureModeController::Get()->camera_controller()->RemoveObserver(this);
}

gfx::Rect CaptureModeSettingsView::GetBounds(
    CaptureModeBarView* capture_mode_bar_view,
    CaptureModeSettingsView* content_view) {
  DCHECK(capture_mode_bar_view);

  const gfx::Size settings_size =
      content_view ? content_view->GetPreferredSize() : kSettingsSize;
  return GetWidgetBounds(capture_mode_bar_view, settings_size);
}

void CaptureModeSettingsView::OnCaptureFolderMayHaveChanged() {
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
  } else if (controller->IsLinuxFilesPath(custom_path)) {
    folder_name =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_LINUX_FILES);
  }

  save_to_menu_group_->AddOrUpdateExistingOption(
      /*option_icon=*/nullptr, folder_name, kCustomFolder);

  controller->CheckFolderAvailability(
      custom_path,
      base::BindOnce(
          &CaptureModeSettingsView::OnCustomFolderAvailabilityChecked,
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeSettingsView::OnDefaultCaptureFolderSelectionChanged() {
  if (save_to_menu_group_)
    save_to_menu_group_->RefreshOptionsSelections();
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureModeSettingsView::GetHighlightableItems() {
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
      highlightable_items;
  DCHECK(audio_input_menu_group_);
  audio_input_menu_group_->AppendHighlightableItems(highlightable_items);
  DCHECK(camera_menu_group_);
  camera_menu_group_->AppendHighlightableItems(highlightable_items);
  if (save_to_menu_group_)
    save_to_menu_group_->AppendHighlightableItems(highlightable_items);
  return highlightable_items;
}

void CaptureModeSettingsView::OnOptionSelected(int option_id) const {
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = controller->camera_controller();
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
    case kCameraOff:
      camera_controller->SetSelectedCamera(CameraId());
      break;
    default:
      DCHECK(!camera_controller->IsCameraDisabledByPolicy());
      DCHECK_GE(option_id, kCameraDevicesBegin);
      const CameraId* camera_id = FindCameraIdByOptionId(option_id);
      DCHECK(camera_id);
      camera_controller->SetSelectedCamera(*camera_id);
      break;
  }
}

bool CaptureModeSettingsView::IsOptionChecked(int option_id) const {
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = controller->camera_controller();
  switch (option_id) {
    case kAudioOff:
      return !CaptureModeController::Get()->GetAudioRecordingEnabled();
    case kAudioMicrophone:
      return CaptureModeController::Get()->GetAudioRecordingEnabled();
    case kDownloadsFolder:
      return GetCurrentCaptureFolder().is_default_downloads_folder ||
             !is_custom_folder_available_.value_or(false);
    case kCustomFolder:
      return !GetCurrentCaptureFolder().is_default_downloads_folder &&
             is_custom_folder_available_.value_or(false);
    case kCameraOff:
      return !camera_controller->selected_camera().is_valid();
    default:
      DCHECK(!camera_controller->IsCameraDisabledByPolicy());
      DCHECK_GE(option_id, kCameraDevicesBegin);
      const CameraId* camera_id = FindCameraIdByOptionId(option_id);
      DCHECK(camera_id);
      return *camera_id == camera_controller->selected_camera();
  }
}

bool CaptureModeSettingsView::IsOptionEnabled(int option_id) const {
  const bool audio_capture_managed_by_policy =
      CaptureModeController::Get()->IsAudioCaptureDisabledByPolicy();
  switch (option_id) {
    case kAudioOff:
      return !audio_capture_managed_by_policy &&
             !capture_mode_session_->is_in_projector_mode();
    case kAudioMicrophone:
      return !audio_capture_managed_by_policy;
    case kCustomFolder:
      return is_custom_folder_available_.value_or(false);
    case kCameraOff: {
      auto* camera_controller =
          CaptureModeController::Get()->camera_controller();
      DCHECK(camera_controller);
      return !camera_controller->IsCameraDisabledByPolicy();
    }
    case kDownloadsFolder:
    default:
      return true;
  }
}

void CaptureModeSettingsView::OnAvailableCamerasChanged(
    const CameraInfoList& cameras) {
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  DCHECK(camera_menu_group_);
  auto* camera_controller = controller->camera_controller();
  DCHECK(camera_controller);
  AddCameraOptions(cameras, camera_controller->IsCameraDisabledByPolicy());

  // If the size of the given `cameras` is equal to the size of the current
  // available cameras, the bounds of the `camera_menu_group_` won't be updated,
  // hence a layout may not be triggered. This can cause the newly added camera
  // options to be not visible. We must guarantee that a layout will always
  // occur by invalidating the layout.
  camera_menu_group_->InvalidateLayout();
  camera_menu_group_->RefreshOptionsSelections();
  capture_mode_session_->MaybeUpdateSettingsBounds();
}

void CaptureModeSettingsView::OnSelectedCameraChanged(
    const CameraId& camera_id) {
  // TODO(conniekxu): Implement this function.
}

void CaptureModeSettingsView::OnSelectFolderMenuItemPressed() {
  capture_mode_session_->OpenFolderSelectionDialog();
}

void CaptureModeSettingsView::OnCustomFolderAvailabilityChecked(
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

const CameraId* CaptureModeSettingsView::FindCameraIdByOptionId(
    int option_id) const {
  auto target_it = option_camera_id_map_.find(option_id);
  if (target_it != option_camera_id_map_.end())
    return &(target_it->second);
  return nullptr;
}

void CaptureModeSettingsView::AddCameraOptions(const CameraInfoList& cameras,
                                               bool managed_by_policy) {
  DCHECK(camera_menu_group_);
  camera_menu_group_->DeleteOptions();
  option_camera_id_map_.clear();
  const bool has_cameras = !cameras.empty();
  if (has_cameras) {
    camera_menu_group_->AddOption(
        /*option_icon=*/nullptr,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_CAMERA_OFF),
        kCameraOff);
    if (!managed_by_policy) {
      int camera_option_id_begin = kCameraDevicesBegin;
      for (const CameraInfo& camera_info : cameras) {
        option_camera_id_map_[camera_option_id_begin] = camera_info.camera_id;
        camera_menu_group_->AddOption(
            /*option_icon=*/nullptr,
            base::UTF8ToUTF16(camera_info.display_name),
            camera_option_id_begin++);
      }
    }
  }
  UpdateCameraMenuGroupVisibility(/*visible=*/has_cameras);
}

void CaptureModeSettingsView::UpdateCameraMenuGroupVisibility(bool visible) {
  separator_1_->SetVisible(visible);
  camera_menu_group_->SetVisible(visible);
}

void CaptureModeSettingsView::OnDemoToolsButtonToggled() {
  const bool was_on = CaptureModeController::Get()->enable_demo_tools();
  CaptureModeController::Get()->EnableDemoTools(!was_on);
}

BEGIN_METADATA(CaptureModeSettingsView, views::View)
END_METADATA

}  // namespace ash
