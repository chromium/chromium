// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager.h"

#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace chromeos {
namespace {

const char kNotificationIdPrefix[] = "vm_camera_mic_manager";
const base::TimeDelta kObserverTimerDelay =
    base::TimeDelta::FromMilliseconds(100);

void OpenCrostiniSettings(Profile* profile) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
}

void OpenPluginVmSettings(Profile* profile) {
  chrome::ShowAppManagementPage(profile, plugin_vm::kPluginVmShelfAppId,
                                AppManagementEntryPoint::kNotificationPluginVm);
}

}  // namespace

constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kNoNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kMicNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kCameraNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kCameraWithMicNotification;

VmCameraMicManager* VmCameraMicManager::Get() {
  static base::NoDestructor<VmCameraMicManager> manager;
  return manager.get();
}

VmCameraMicManager::VmCameraMicManager()
    : observer_timer_(
          FROM_HERE,
          kObserverTimerDelay,
          base::BindRepeating(&VmCameraMicManager::NotifyActiveChanged,
                              // Unretained because the timer cannot
                              // live longer than the manager.
                              base::Unretained(this))) {}

void VmCameraMicManager::OnPrimaryUserSessionStarted(Profile* primary_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  primary_profile_ = primary_profile;
  crostini_vm_notification_observer_.Initialize(
      primary_profile_, base::BindRepeating(OpenCrostiniSettings));
  plugin_vm_notification_observer_.Initialize(
      primary_profile_, base::BindRepeating(OpenPluginVmSettings));

  for (VmType vm : {VmType::kCrostiniVm, VmType::kPluginVm}) {
    vm_info_map_[vm] = {};
  }

  // Only do the subscription in real ChromeOS environment.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(media::ShouldUseCrosCameraService),
        base::BindOnce(&VmCameraMicManager::MaybeSubscribeToCameraService,
                       base::Unretained(this)));

    CrasAudioHandler::Get()->AddAudioObserver(this);
    // Fetch the current value.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VmCameraMicManager::OnNumberOfInputStreamsWithPermissionChanged,
            base::Unretained(this)));
  }
}

// The class is supposed to be used as a singleton with `base::NoDestructor`, so
// we do not do clean up (e.g. deregister as observers) here.
VmCameraMicManager::~VmCameraMicManager() = default;

void VmCameraMicManager::MaybeSubscribeToCameraService(
    bool should_use_cros_camera_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!should_use_cros_camera_service) {
    return;
  }

  auto* camera = media::CameraHalDispatcherImpl::GetInstance();
  // OnActiveClientChange() will be called automatically after the
  // subscription, so there is no need to get the current status here.
  camera->AddActiveClientObserver(this);
  OnCameraPrivacySwitchStatusChanged(
      camera->AddCameraPrivacySwitchObserver(this));
}

void VmCameraMicManager::UpdateVmInfoAndNotifications(
    VmType vm,
    void (VmInfo::*updator)(bool),
    bool value) {
  auto it = vm_info_map_.find(vm);
  CHECK(it != vm_info_map_.end());
  auto& vm_info = it->second;

  const NotificationType old_notification_type = vm_info.notification_type();
  (vm_info.*updator)(value);
  const NotificationType new_notification_type = vm_info.notification_type();

  if (old_notification_type == new_notification_type)
    return;

  if (!observer_timer_.IsRunning()) {
    observer_timer_.Reset();
  }

  // We always show 0 or 1 notifications for a VM, so here we just need to close
  // the previous one if it exists and open the new one if necessary.
  if (old_notification_type != kNoNotification) {
    CloseNotification(vm, old_notification_type);
  }
  if (new_notification_type != kNoNotification) {
    OpenNotification(vm, new_notification_type);
  }
}

bool VmCameraMicManager::IsDeviceActive(DeviceType device) const {
  for (const auto& vm_info : vm_info_map_) {
    const NotificationType& notification_type =
        vm_info.second.notification_type();
    if (notification_type[static_cast<size_t>(device)]) {
      return true;
    }
  }
  return false;
}

bool VmCameraMicManager::IsNotificationActive(DeviceType device) const {
  for (const auto& vm_info : vm_info_map_) {
    const NotificationType& notification_type =
        vm_info.second.notification_type();
    switch (device) {
      case DeviceType::kMic:
        if (notification_type == kMicNotification) {
          return true;
        }
        break;
      case DeviceType::kCamera:
        // Both the "camera only" and "camera and mic" notifications use the
        // camera icon.
        if (notification_type[static_cast<size_t>(DeviceType::kCamera)]) {
          return true;
        }
        break;
    }
  }
  return false;
}

void VmCameraMicManager::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_active) {
  // Crostini does not support camera yet.

  if (type == cros::mojom::CameraClientType::PLUGINVM) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&VmCameraMicManager::UpdateVmInfoAndNotifications,
                       base::Unretained(this), VmType::kPluginVm,
                       &VmInfo::SetCameraAccessing, is_active));
  }
}

void VmCameraMicManager::OnCameraPrivacySwitchStatusChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  using cros::mojom::CameraPrivacySwitchState;
  bool is_on;
  switch (state) {
    case CameraPrivacySwitchState::UNKNOWN:
    case CameraPrivacySwitchState::OFF:
      is_on = false;
      break;
    case CameraPrivacySwitchState::ON:
      is_on = true;
      break;
  }

  DCHECK(!vm_info_map_.empty());
  for (auto& vm_and_info : vm_info_map_) {
    VmType vm = vm_and_info.first;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&VmCameraMicManager::UpdateVmInfoAndNotifications,
                       base::Unretained(this), vm,
                       &VmInfo::SetCameraPrivacyIsOn, is_on));
  }
}

void VmCameraMicManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VmCameraMicManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VmCameraMicManager::NotifyActiveChanged() {
  for (Observer& observer : observers_) {
    observer.OnVmCameraMicActiveChanged(this);
  }
}

std::string VmCameraMicManager::GetNotificationId(VmType vm,
                                                  NotificationType type) {
  std::string id = kNotificationIdPrefix;

  switch (vm) {
    case VmType::kCrostiniVm:
      id.append("-crostini");
      break;
    case VmType::kPluginVm:
      id.append("-pluginvm");
      break;
  }

  id.append(type.to_string());

  return id;
}

void VmCameraMicManager::OpenNotification(VmType vm, NotificationType type) {
  DCHECK_NE(type, kNoNotification);
  if (!base::FeatureList::IsEnabled(
          features::kVmCameraMicIndicatorsAndNotifications)) {
    return;
  }

  const gfx::VectorIcon* source_icon = nullptr;
  int message_id;
  if (type[static_cast<size_t>(DeviceType::kCamera)]) {
    source_icon = &::vector_icons::kVideocamIcon;
    if (type[static_cast<size_t>(DeviceType::kMic)]) {
      message_id = IDS_APP_USING_CAMERA_MIC_NOTIFICATION_MESSAGE;
    } else {
      message_id = IDS_APP_USING_CAMERA_NOTIFICATION_MESSAGE;
    }
  } else {
    DCHECK_EQ(type, kMicNotification);
    source_icon = &::vector_icons::kMicIcon;
    message_id = IDS_APP_USING_MIC_NOTIFICATION_MESSAGE;
  }

  int app_name_id;
  base::WeakPtr<message_center::NotificationObserver> notification_observer_;
  switch (vm) {
    case VmCameraMicManager::VmType::kCrostiniVm:
      app_name_id = IDS_CROSTINI_LINUX;
      notification_observer_ = crostini_vm_notification_observer_.GetWeakPtr();
      break;
    case VmCameraMicManager::VmType::kPluginVm:
      app_name_id = IDS_PLUGIN_VM_APP_NAME;
      notification_observer_ = plugin_vm_notification_observer_.GetWeakPtr();
      break;
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = source_icon;
  rich_notification_data.pinned = true;
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationId(vm, type),
      /*title=*/
      l10n_util::GetStringFUTF16(message_id,
                                 l10n_util::GetStringUTF16(app_name_id)),
      /*message=*/base::string16(),
      /*icon=*/gfx::Image(),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_CHROME_OS_NOTIFICATION_SOURCE),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 ash::kVmCameraMicNotifierId),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          std::move(notification_observer_)));

  NotificationDisplayService::GetForProfile(primary_profile_)
      ->Display(NotificationHandler::Type::TRANSIENT, notification,
                /*metadata=*/nullptr);
}

void VmCameraMicManager::CloseNotification(VmType vm, NotificationType type) {
  DCHECK_NE(type, kNoNotification);
  if (!base::FeatureList::IsEnabled(
          features::kVmCameraMicIndicatorsAndNotifications)) {
    return;
  }
  NotificationDisplayService::GetForProfile(primary_profile_)
      ->Close(NotificationHandler::Type::TRANSIENT,
              GetNotificationId(vm, type));
}

VmCameraMicManager::VmInfo::VmInfo() = default;
VmCameraMicManager::VmInfo::VmInfo(const VmInfo&) = default;
VmCameraMicManager::VmInfo::~VmInfo() = default;

void VmCameraMicManager::VmInfo::SetMicActive(bool active) {
  notification_type_.set(static_cast<size_t>(DeviceType::kMic), active);
}

void VmCameraMicManager::VmInfo::SetCameraAccessing(bool accessing) {
  camera_accessing_ = accessing;
  OnCameraUpdated();
}

void VmCameraMicManager::VmInfo::SetCameraPrivacyIsOn(bool on) {
  camera_privacy_is_on_ = on;
  OnCameraUpdated();
}

void VmCameraMicManager::VmInfo::OnCameraUpdated() {
  notification_type_.set(static_cast<size_t>(DeviceType::kCamera),
                         camera_accessing_ && !camera_privacy_is_on_);
}

VmCameraMicManager::VmNotificationObserver::VmNotificationObserver() = default;
VmCameraMicManager::VmNotificationObserver::~VmNotificationObserver() = default;

void VmCameraMicManager::VmNotificationObserver::Initialize(
    Profile* profile,
    OpenSettingsFunction open_settings) {
  profile_ = profile;
  open_settings_ = std::move(open_settings);
}

base::WeakPtr<message_center::NotificationObserver>
VmCameraMicManager::VmNotificationObserver::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VmCameraMicManager::VmNotificationObserver::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  // We only have one button --- the settings button.
  open_settings_.Run(profile_);
}

void VmCameraMicManager::OnNumberOfInputStreamsWithPermissionChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& clients_and_numbers =
      CrasAudioHandler::Get()->GetNumberOfInputStreamsWithPermission();

  auto update = [&](CrasAudioHandler::ClientType cras_client_type, VmType vm) {
    auto it = clients_and_numbers.find(cras_client_type);
    bool active = (it != clients_and_numbers.end() && it->second != 0);

    UpdateVmInfoAndNotifications(vm, &VmInfo::SetMicActive, active);
  };

  update(CrasAudioHandler::ClientType::VM_TERMINA, VmType::kCrostiniVm);
  update(CrasAudioHandler::ClientType::VM_PLUGIN, VmType::kPluginVm);
}

}  // namespace chromeos
