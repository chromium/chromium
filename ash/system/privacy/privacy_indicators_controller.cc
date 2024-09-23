// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

PrivacyIndicatorsController* g_controller_instance = nullptr;

// Create a notification with the customized metadata for privacy indicators.
std::unique_ptr<message_center::Notification>
CreatePrivacyIndicatorsNotification(
    const std::string& app_id,
    std::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate) {
  std::u16string app_name_str = app_name.value_or(l10n_util::GetStringUTF16(
      IDS_PRIVACY_NOTIFICATION_MESSAGE_DEFAULT_APP_NAME));

  std::u16string title;
  std::u16string message;
  const gfx::VectorIcon* app_icon;
  if (is_camera_used && is_microphone_used) {
    title = l10n_util::GetStringFUTF16(
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC, app_name_str);
    app_icon = &kPrivacyIndicatorsIcon;
  } else if (is_camera_used) {
    title = l10n_util::GetStringFUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
                                       app_name_str);
    app_icon = &kPrivacyIndicatorsCameraIcon;
  } else {
    title = l10n_util::GetStringFUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC,
                                       app_name_str);
    app_icon = &kPrivacyIndicatorsMicrophoneIcon;
  }

  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  // Make the notification low priority so that it is silently added (no popup).
  optional_fields.priority = message_center::LOW_PRIORITY;

  optional_fields.parent_vector_small_image = &kPrivacyIndicatorsIcon;

  if (delegate->launch_settings_callback()) {
    optional_fields.buttons.emplace_back(
        features::AreOngoingProcessesEnabled()
            ? message_center::ButtonInfo(
                  /*vector_icon=*/&kSettingsIcon,
                  /*accessible_name=*/l10n_util::GetStringUTF16(
                      IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS))
            : message_center::ButtonInfo(l10n_util::GetStringUTF16(
                  IDS_PRIVACY_NOTIFICATION_BUTTON_APP_SETTINGS)));
  }

  auto notification = CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      GetPrivacyIndicatorsNotificationId(app_id), title, std::u16string(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      /*delegate=*/delegate, *app_icon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);

  return notification;
}

// Adds, updates, or removes the privacy notification associated with the given
// `app_id`.
void ModifyPrivacyIndicatorsNotification(
    const std::string& app_id,
    std::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate) {
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  auto* message_center = message_center::MessageCenter::Get();
  std::string id = GetPrivacyIndicatorsNotificationId(app_id);
  bool notification_exists = message_center->FindNotificationById(id);

  if (!is_camera_used && !is_microphone_used) {
    if (notification_exists)
      message_center->RemoveNotification(id, /*by_user=*/false);
    return;
  }

  auto notification = CreatePrivacyIndicatorsNotification(
      app_id, app_name, is_camera_used, is_microphone_used, delegate);
  if (notification_exists) {
    message_center->UpdateNotification(id, std::move(notification));
    return;
  }
  message_center->AddNotification(std::move(notification));
}

// Updates the `PrivacyIndicatorsTrayItemView` across all status area widgets.
void UpdatePrivacyIndicatorsView(bool is_camera_used,
                                 bool is_microphone_used,
                                 bool is_new_app,
                                 bool was_camera_in_use,
                                 bool was_microphone_in_use) {
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  DCHECK(Shell::HasInstance());
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
    DCHECK(status_area_widget);

    auto* privacy_indicators_view =
        status_area_widget->notification_center_tray()
            ->privacy_indicators_view();

    DCHECK(privacy_indicators_view);
    privacy_indicators_view->OnCameraAndMicrophoneAccessStateChanged(
        is_camera_used, is_microphone_used, is_new_app, was_camera_in_use,
        was_microphone_in_use);
  }
}

// Updates the access status of `app_id` for the given `access_map`.
void UpdateAccessStatus(
    const std::string& app_id,
    bool is_accessed,
    std::map<std::string, ash::PrivacyIndicatorsAppInfo>& access_map,
    std::optional<std::u16string> app_name,
    scoped_refptr<ash::PrivacyIndicatorsNotificationDelegate> delegate) {
  if (access_map.contains(app_id) == is_accessed) {
    return;
  }

  if (is_accessed) {
    ash::PrivacyIndicatorsAppInfo info;
    info.app_name = app_name;
    info.delegate = delegate;
    access_map[app_id] = std::move(info);
  } else {
    access_map.erase(app_id);
  }
}

void UpdatePrivacyIndicatorsVisibility() {
  DCHECK(Shell::HasInstance());
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    CHECK(root_window_controller);
    auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
    CHECK(status_area_widget);

    auto* privacy_indicators_view =
        status_area_widget->notification_center_tray()
            ->privacy_indicators_view();
    CHECK(privacy_indicators_view);

    privacy_indicators_view->UpdateVisibility();
  }
}

}  // namespace

PrivacyIndicatorsNotificationDelegate::PrivacyIndicatorsNotificationDelegate(
    std::optional<base::RepeatingClosure> launch_settings_callback)
    : launch_settings_callback_(launch_settings_callback) {}

PrivacyIndicatorsNotificationDelegate::
    ~PrivacyIndicatorsNotificationDelegate() = default;

void PrivacyIndicatorsNotificationDelegate::SetLaunchSettingsCallback(
    const base::RepeatingClosure& launch_settings_callback) {
  launch_settings_callback_ = launch_settings_callback;
}

void PrivacyIndicatorsNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index) {
    // Click on the notification body should launch app settings if possible.
    if (launch_settings_callback_) {
      launch_settings_callback_->Run();
    }
    return;
  }

  CHECK(*button_index == 0);
  if (launch_settings_callback_) {
    launch_settings_callback_->Run();
  }
}

std::string GetPrivacyIndicatorsNotificationId(const std::string& app_id) {
  return kPrivacyIndicatorsNotificationIdPrefix + app_id;
}

PrivacyIndicatorsAppInfo::PrivacyIndicatorsAppInfo() = default;

PrivacyIndicatorsAppInfo::~PrivacyIndicatorsAppInfo() = default;

PrivacyIndicatorsController::PrivacyIndicatorsController() {
  DCHECK(!g_controller_instance);
  g_controller_instance = this;

  CrasAudioHandler::Get()->AddAudioObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()->AddCameraPrivacySwitchObserver(
      this);
}

PrivacyIndicatorsController::~PrivacyIndicatorsController() {
  DCHECK_EQ(this, g_controller_instance);
  g_controller_instance = nullptr;

  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
}

// static
PrivacyIndicatorsController* PrivacyIndicatorsController::Get() {
  return g_controller_instance;
}

void PrivacyIndicatorsController::UpdatePrivacyIndicators(
    const std::string& app_id,
    std::optional<std::u16string> app_name,
    bool is_camera_used,
    bool is_microphone_used,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate,
    PrivacyIndicatorsSource source) {
  const bool is_new_app = !apps_using_camera_.contains(app_id) &&
                          !apps_using_microphone_.contains(app_id);
  const bool was_camera_in_use = IsCameraUsed();
  const bool was_microphone_in_use = IsMicrophoneUsed();

  UpdateAccessStatus(app_id, /*is_accessed=*/is_camera_used,
                     /*access_map=*/apps_using_camera_, app_name, delegate);
  UpdateAccessStatus(app_id,
                     /*is_accessed=*/is_microphone_used,
                     /*access_map=*/apps_using_microphone_, app_name, delegate);

  is_camera_used = is_camera_used && !camera_muted_by_hardware_switch_ &&
                   !camera_muted_by_software_switch_;
  is_microphone_used =
      is_microphone_used && !CrasAudioHandler::Get()->IsInputMuted();

  if (!is_camera_used && !is_microphone_used) {
    indicator_hiding_delay_timer_.Start(
        FROM_HERE, CalculateIndicatorsDelayTime(),
        base::BindOnce(&PrivacyIndicatorsController::TriggerPrivacyIndicators,
                       base::Unretained(this), is_camera_used,
                       is_microphone_used, is_new_app, was_camera_in_use,
                       was_microphone_in_use, app_id, app_name, delegate));
  } else {
    if (!ShouldSkipShowPrivacyIndicators(is_camera_used, is_microphone_used,
                                         is_new_app, was_camera_in_use)) {
      privacy_indicator_time_ = base::TimeTicks::Now();
      recent_active_state_ = {is_camera_used, is_microphone_used};
      TriggerPrivacyIndicators(is_camera_used, is_microphone_used, is_new_app,
                               was_camera_in_use, was_microphone_in_use, app_id,
                               app_name, delegate);
    }
    indicator_hiding_delay_timer_.Stop();
  }

  base::UmaHistogramEnumeration("Ash.PrivacyIndicators.Source", source);
}

base::TimeDelta PrivacyIndicatorsController::CalculateIndicatorsDelayTime()
    const {
  base::TimeDelta delay;
  const base::TimeDelta indicator_display_time =
      base::TimeTicks::Now() - privacy_indicator_time_;
  if (indicator_display_time < kPrivacyIndicatorsMinimumHoldDuration) {
    delay =
        std::max(kPrivacyIndicatorsMinimumHoldDuration - indicator_display_time,
                 kPrivacyIndicatorsHoldAfterUseDuration);
  } else {
    delay = kPrivacyIndicatorsHoldAfterUseDuration;
  }
  return delay;
}

// Determines whether to skip showing privacy indicators for a new app.
// This is necessary to prevent flickering in scenarios where:
// 1. Another app is currently using the camera (`was_camera_in_use`).
// 2. The new app (`is_new_app`) is attempting to access the
// camera(`is_camera_used`).
// 3. The camera/mic state matches the recent active state
// (`recent_active_state_`).
// 4. The indicator hiding timer is running, indicating a recent disconnection.
//
// In such conflicts, we avoid constantly canceling and restarting the hiding
// delay timer for every retry attempt by the new app, ensuring a smoother user
// experience.
bool PrivacyIndicatorsController::ShouldSkipShowPrivacyIndicators(
    bool is_camera_used,
    bool is_microphone_used,
    bool is_new_app,
    bool was_camera_in_use) const {
  return was_camera_in_use && is_new_app && is_camera_used &&
         (recent_active_state_ ==
          std::make_pair(is_camera_used, is_microphone_used)) &&
         indicator_hiding_delay_timer_.IsRunning();
}

void PrivacyIndicatorsController::TriggerPrivacyIndicators(
    bool is_camera_used,
    bool is_microphone_used,
    bool is_new_app,
    bool was_camera_in_use,
    bool was_microphone_in_use,
    const std::string& app_id,
    std::optional<std::u16string> app_name,
    scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate) {
  ModifyPrivacyIndicatorsNotification(app_id, app_name, is_camera_used,
                                      is_microphone_used, delegate);
  UpdatePrivacyIndicatorsView(is_camera_used, is_microphone_used, is_new_app,
                              was_camera_in_use, was_microphone_in_use);
}

void PrivacyIndicatorsController::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state) {
  camera_muted_by_hardware_switch_ =
      state == cros::mojom::CameraPrivacySwitchState::ON;

  UpdateForCameraMuteStateChanged();
}

void PrivacyIndicatorsController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  camera_muted_by_software_switch_ =
      state == cros::mojom::CameraPrivacySwitchState::ON;

  UpdateForCameraMuteStateChanged();
}

void PrivacyIndicatorsController::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  // Iterate through all the apps that are tracked as using the microphone, then
  // modify the notification according to the mute state of the microphone.
  for (const auto& [app_id, app_info] : apps_using_microphone_) {
    // Retrieve camera usage state for each individual app to update in the
    // notification.
    bool is_camera_used = apps_using_camera_.contains(app_id) &&
                          !camera_muted_by_hardware_switch_ &&
                          !camera_muted_by_software_switch_;
    ModifyPrivacyIndicatorsNotification(
        app_id, app_info.app_name, is_camera_used,
        /*is_microphone_used=*/!mute_on, app_info.delegate);
  }

  UpdatePrivacyIndicatorsVisibility();
}

void PrivacyIndicatorsController::UpdateForCameraMuteStateChanged() {
  // Iterate through all the apps that are tracked as using the camera, then
  // modify the notification according to the mute state of camera.
  for (const auto& [app_id, app_info] : apps_using_camera_) {
    // Retrieve microphone usage state for each individual app to update in the
    // notification.
    bool is_camera_used =
        !camera_muted_by_hardware_switch_ && !camera_muted_by_software_switch_;
    bool is_microphone_used = apps_using_microphone_.contains(app_id) &&
                              !CrasAudioHandler::Get()->IsInputMuted();
    ModifyPrivacyIndicatorsNotification(app_id, app_info.app_name,
                                        is_camera_used, is_microphone_used,
                                        app_info.delegate);
  }

  UpdatePrivacyIndicatorsVisibility();
}

bool PrivacyIndicatorsController::IsCameraUsed() const {
  return !apps_using_camera_.empty() && !camera_muted_by_hardware_switch_ &&
         !camera_muted_by_software_switch_;
}

bool PrivacyIndicatorsController::IsMicrophoneUsed() const {
  return !apps_using_microphone_.empty() &&
         !CrasAudioHandler::Get()->IsInputMuted();
}

void UpdatePrivacyIndicatorsScreenShareStatus(bool is_screen_sharing) {
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  DCHECK(Shell::HasInstance());
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
    DCHECK(status_area_widget);

    auto* privacy_indicators_view =
        status_area_widget->notification_center_tray()
            ->privacy_indicators_view();

    DCHECK(privacy_indicators_view);

    privacy_indicators_view->UpdateScreenShareStatus(is_screen_sharing);
  }
}

}  // namespace ash