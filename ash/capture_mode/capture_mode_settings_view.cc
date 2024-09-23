// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_settings_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_toggle_button.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 10;
constexpr gfx::RoundedCornersF kRoundedCorners{kCornerRadius};

constexpr gfx::Size kSettingsSize{266, 248};

// Returns the bounds of the settings widget in screen coordinates relative to
// the bounds of the `bar_view` based on its given preferred
// `settings_size`, which should be centered with respect to the capture
// bar.
//
// The bounds priority works as follows:
// - If there is enough space above the bar view, we will show the menu at its
//   full height.
// - Otherwise, we will choose between showing above or below the bar,
//   whichever has more space. The available space only includes the work area,
//   as we do not want to show the menu on top of or behind the shelf.
// - If necessary, we will also constrain the height of the menu, up to
//   `capture_mode::kSettingsMenuMinHeight`.
gfx::Rect GetWidgetBounds(CaptureModeBarView* bar_view,
                          const gfx::Size& settings_size) {
  const int width = settings_size.width();
  const int pref_height = settings_size.height();

  const gfx::Rect bar_bounds = bar_view->GetBoundsInScreen();
  const int x = bar_bounds.CenterPoint().x() - width / 2.f;
  int menu_bottom =
      bar_bounds.y() - capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu;
  int y = menu_bottom - pref_height;

  // Showing the menu above the bar at full height is our priority, but this may
  // change if it is too close to the top of the screen.
  if (y < capture_mode::kMinDistanceFromSettingsToScreen) {
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            bar_view->GetWidget()->GetNativeWindow());
    const int available_above = menu_bottom -
                                capture_mode::kMinDistanceFromSettingsToScreen -
                                work_area.y();
    const int available_below =
        work_area.y() + work_area.height() -
        capture_mode::kMinDistanceFromSettingsToScreen -
        capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu - bar_bounds.y() -
        bar_bounds.height();

    // We want to show the menu on the side of the bar that has more space.
    if (available_above >= available_below) {
      y = std::max(
          bar_bounds.y() -
              capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu -
              pref_height,
          work_area.y() + capture_mode::kMinDistanceFromSettingsToScreen);
      menu_bottom =
          bar_bounds.y() - capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu;
    } else {
      y = bar_bounds.bottom() +
          capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu;
      menu_bottom = std::min(
          y + pref_height,
          work_area.bottom() - capture_mode::kMinDistanceFromSettingsToScreen);
    }
  }

  return gfx::Rect(
      x, y, width,
      std::max(capture_mode::kSettingsMenuMinHeight, menu_bottom - y));
}

CaptureModeController::CaptureFolder GetCurrentCaptureFolder() {
  return CaptureModeController::Get()->GetCurrentCaptureFolder();
}

}  // namespace

CaptureModeSettingsView::CaptureModeSettingsView(
    CaptureModeSession* session,
    CaptureModeBehavior* active_behavior)
    : ScrollView(views::ScrollView::ScrollWithLayers::kEnabled),
      capture_mode_session_(session),
      active_behavior_(active_behavior),
      shadow_(SystemShadow::CreateShadowOnNinePatchLayerForView(
          this,
          SystemShadow::Type::kElevation12)) {
  auto* controller = CaptureModeController::Get();

  SetContents(std::make_unique<views::View>());

  if (controller->can_start_new_recording()) {
    const bool audio_capture_managed_by_policy =
        controller->IsAudioCaptureDisabledByPolicy();

    DCHECK(
        !audio_capture_managed_by_policy ||
        active_behavior->SupportsAudioRecordingMode(AudioRecordingMode::kOff))
        << "A client session should not be allowed to begin if audio "
           "recording is diabled by policy.";

    audio_input_menu_group_ =
        contents()->AddChildView(std::make_unique<CaptureModeMenuGroup>(
            this, kCaptureModeMicIcon,
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT),
            audio_capture_managed_by_policy));

    // A list of all the possible audio options.
    struct {
      // The backend audio recording mode for this option.
      AudioRecordingMode audio_recording_mode;
      // The ID of this menu group option.
      int option_id;
      // The ID of the string that will be used for the option's label.
      int string_id;
      // True if the option can be added if audio recording is managed by an
      // admin policy.
      bool add_if_managed_by_policy;
    } kAudioOptions[] = {
        {AudioRecordingMode::kOff, kAudioOff,
         IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_OFF,
         /*add_if_managed_by_policy=*/true},
        {AudioRecordingMode::kSystem, kAudioSystem,
         IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_SYSTEM,
         /*add_if_managed_by_policy=*/false},
        {AudioRecordingMode::kMicrophone, kAudioMicrophone,
         IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_MICROPHONE,
         /*add_if_managed_by_policy=*/false},
        {AudioRecordingMode::kSystemAndMicrophone, kAudioSystemAndMicrophone,
         IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT_SYSTEM_AND_MICROPHONE,
         /*add_if_managed_by_policy=*/false},
    };

    for (const auto& audio_option : kAudioOptions) {
      if ((!audio_capture_managed_by_policy ||
           audio_option.add_if_managed_by_policy) &&
          active_behavior->SupportsAudioRecordingMode(
              audio_option.audio_recording_mode)) {
        audio_input_menu_group_->AddOption(
            /*option_icon=*/nullptr,
            l10n_util::GetStringUTF16(audio_option.string_id),
            audio_option.option_id);
      }
    }

    separator_1_ =
        contents()->AddChildView(std::make_unique<views::Separator>());
    separator_1_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    auto* camera_controller = controller->camera_controller();
    const bool camera_managed_by_policy =
        camera_controller->IsCameraDisabledByPolicy();
    // Even if the camera feature is managed by policy, we still want to observe
    // the camera controller, since we need to be notified with camera additions
    // and removals, which affect the visibility of the `camera_menu_group_`.
    camera_controller->AddObserver(this);
    camera_menu_group_ =
        contents()->AddChildView(std::make_unique<CaptureModeMenuGroup>(
            this, kCaptureModeCameraIcon,
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_CAMERA),
            camera_managed_by_policy));

    AddCameraOptions(camera_controller->available_cameras(),
                     camera_managed_by_policy);
  }

  if (controller->can_start_new_recording()) {
    separator_2_ =
        contents()->AddChildView(std::make_unique<views::Separator>());
    separator_2_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    demo_tools_menu_toggle_button_ =
        contents()->AddChildView(std::make_unique<CaptureModeMenuToggleButton>(
            kCaptureModeDemoToolsSettingsMenuEntryPointIcon,
            l10n_util::GetStringUTF16(
                IDS_ASH_SCREEN_CAPTURE_DEMO_TOOLS_SHOW_CLICKS_AND_KEYS),
            CaptureModeController::Get()->enable_demo_tools(),
            base::BindRepeating(
                &CaptureModeSettingsView::OnDemoToolsButtonToggled,
                base::Unretained(this))));
  }

  if (active_behavior->ShouldSaveToSettingsBeIncluded()) {
    separator_3_ =
        contents()->AddChildView(std::make_unique<views::Separator>());
    separator_3_->SetColorId(ui::kColorAshSystemUIMenuSeparator);

    const bool custom_folder_managed_by_policy =
        controller->IsCustomFolderManagedByPolicy();
    save_to_menu_group_ =
        contents()->AddChildView(std::make_unique<CaptureModeMenuGroup>(
            this, kCaptureModeFolderIcon,
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO),
            /*managed=*/custom_folder_managed_by_policy));
    save_to_menu_group_->AddOption(
        /*option_icon=*/nullptr,
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_DOWNLOADS),
        kDownloadsFolder);
    save_to_menu_group_->AddMenuItem(
        base::BindRepeating(
            &CaptureModeSettingsView::OnSelectFolderMenuItemPressed,
            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_SELECT_FOLDER),
        /*enabled=*/!custom_folder_managed_by_policy);
  }

  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kRoundedCorners);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  // The options should appear vertically stacked on top of each other.
  contents()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  capture_mode_util::SetHighlightBorder(
      this, kCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow);

  shadow_->SetRoundedCornerRadius(kCornerRadius);
}

CaptureModeSettingsView::~CaptureModeSettingsView() {
  CaptureModeController::Get()->camera_controller()->RemoveObserver(this);
}

// static
gfx::Rect CaptureModeSettingsView::GetBounds(
    CaptureModeBarView* capture_mode_bar_view,
    CaptureModeSettingsView* settings_view) {
  DCHECK(capture_mode_bar_view);

  const gfx::Size settings_size =
      settings_view ? settings_view->GetPreferredSize() : kSettingsSize;
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
  } else if (controller->IsRootOneDriveFilesPath(custom_path)) {
    folder_name =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_ONE_DRIVE);
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

  if (demo_tools_menu_toggle_button_) {
    highlightable_items.push_back(
        CaptureModeSessionFocusCycler::HighlightHelper::Get(
            demo_tools_menu_toggle_button_->toggle_button()));
  }

  if (save_to_menu_group_) {
    save_to_menu_group_->AppendHighlightableItems(highlightable_items);
  }

  return highlightable_items;
}

void CaptureModeSettingsView::OnOptionSelected(int option_id) const {
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = controller->camera_controller();
  switch (option_id) {
    case kAudioOff:
      controller->SetAudioRecordingMode(AudioRecordingMode::kOff);
      break;
    case kAudioSystem:
      controller->SetAudioRecordingMode(AudioRecordingMode::kSystem);
      break;
    case kAudioMicrophone:
      controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
      break;
    case kAudioSystemAndMicrophone:
      controller->SetAudioRecordingMode(
          AudioRecordingMode::kSystemAndMicrophone);
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
      camera_controller->SetSelectedCamera(CameraId(), /*by_user=*/true);
      break;
    default:
      DCHECK(!camera_controller->IsCameraDisabledByPolicy());
      DCHECK_GE(option_id, kCameraDevicesBegin);
      const CameraId* camera_id = FindCameraIdByOptionId(option_id);
      DCHECK(camera_id);
      camera_controller->SetSelectedCamera(*camera_id, /*by_user=*/true);
      break;
  }
}

bool CaptureModeSettingsView::IsOptionChecked(int option_id) const {
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = controller->camera_controller();
  const auto effective_audio_mode =
      controller->GetEffectiveAudioRecordingMode();
  const bool is_custom_folder =
      !GetCurrentCaptureFolder().is_default_downloads_folder &&
      (controller->IsCustomFolderManagedByPolicy() ||
       is_custom_folder_available_.value_or(false));
  switch (option_id) {
    case kAudioOff:
      return effective_audio_mode == AudioRecordingMode::kOff;
    case kAudioSystem:
      return effective_audio_mode == AudioRecordingMode::kSystem;
    case kAudioMicrophone:
      return effective_audio_mode == AudioRecordingMode::kMicrophone;
    case kAudioSystemAndMicrophone:
      return effective_audio_mode == AudioRecordingMode::kSystemAndMicrophone;
    case kDownloadsFolder:
      return !is_custom_folder;
    case kCustomFolder:
      return is_custom_folder;
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
  auto* controller = CaptureModeController::Get();
  const bool audio_capture_managed_by_policy =
      controller->IsAudioCaptureDisabledByPolicy();
  switch (option_id) {
    case kAudioOff:
      return !audio_capture_managed_by_policy &&
             active_behavior_->SupportsAudioRecordingMode(
                 AudioRecordingMode::kOff);
    case kAudioSystem:
    case kAudioMicrophone:
    case kAudioSystemAndMicrophone:
      return !audio_capture_managed_by_policy;
    case kCustomFolder:
      return is_custom_folder_available_.value_or(false) ||
             controller->IsCustomFolderManagedByPolicy();
    case kCameraOff: {
      auto* camera_controller = controller->camera_controller();
      DCHECK(camera_controller);
      return !camera_controller->IsCameraDisabledByPolicy();
    }
    case kDownloadsFolder:
      return !controller->IsCustomFolderManagedByPolicy();
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
  CaptureModeController::Get()->EnableDemoTools(/*enable=*/!was_on);
}

BEGIN_METADATA(CaptureModeSettingsView)
END_METADATA

}  // namespace ash
