// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager.h"

#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/camera_mic/vm_camera_mic_manager_factory.h"
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

VmCameraMicManager* VmCameraMicManager::GetForProfile(Profile* profile) {
  return VmCameraMicManagerFactory::GetForProfile(profile);
}

VmCameraMicManager::VmCameraMicManager(Profile* profile)
    : profile_(profile),
      crostini_vm_notification_observer_(
          profile,
          base::BindRepeating(OpenCrostiniSettings)),
      plugin_vm_notification_observer_(
          profile,
          base::BindRepeating(OpenPluginVmSettings)),
      observer_timer_(
          FROM_HERE,
          kObserverTimerDelay,
          base::BindRepeating(&VmCameraMicManager::NotifyActiveChanged,
                              // Unretained because the timer cannot
                              // live longer than the manager.
                              base::Unretained(this))) {
  DCHECK(ProfileHelper::IsPrimaryProfile(profile));

  for (VmType vm : {VmType::kCrostiniVm, VmType::kPluginVm}) {
    notification_map_[vm] = {};
  }

  // Camera service does not behave in non ChromeOS environment (e.g. testing,
  // linux chromeos).
  if (base::SysInfo::IsRunningOnChromeOS() &&
      media::ShouldUseCrosCameraService()) {
    // OnActiveClientChange() will be called automatically after the
    // subscription, so there is no need to get the current status here.
    camera_observation_.Observe(media::CameraHalDispatcherImpl::GetInstance());
  }
}

VmCameraMicManager::~VmCameraMicManager() = default;

void VmCameraMicManager::SetActive(VmType vm, DeviceType device, bool active) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotificationType& notification_type = notification_map_[vm];
  const NotificationType old_notification_type = notification_type;
  notification_type.set(static_cast<size_t>(device), active);
  if (old_notification_type == notification_type)
    return;

  if (!observer_timer_.IsRunning()) {
    observer_timer_.Reset();
  }

  // We always show 0 or 1 notifications for a VM, so here we just need to close
  // the previous one if it exists and open the new one if necessary.
  if (old_notification_type != kNoNotification) {
    CloseNotification(vm, old_notification_type);
  }
  if (notification_type != kNoNotification) {
    OpenNotification(vm, notification_type);
  }
}

bool VmCameraMicManager::IsDeviceActive(DeviceType device) const {
  for (const auto& vm_notification : notification_map_) {
    const NotificationType& notification_type = vm_notification.second;
    if (notification_type[static_cast<size_t>(device)]) {
      return true;
    }
  }
  return false;
}

bool VmCameraMicManager::IsNotificationActive(DeviceType device) const {
  for (const auto& vm_notification : notification_map_) {
    const NotificationType& notification_type = vm_notification.second;
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
  // TODO(b/167491603): `UNKNOWN` is for Parallels using v0 camera API. We
  // should be able to remove it soon.
  if (type == cros::mojom::CameraClientType::UNKNOWN ||
      type == cros::mojom::CameraClientType::PLUGINVM) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&VmCameraMicManager::SetActive,
                       weak_ptr_factory_.GetWeakPtr(), VmType::kPluginVm,
                       DeviceType::kCamera, is_active));
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

  const char* notifier_id = ash::kVmCameraMicNotifierId;

  const gfx::VectorIcon* source_icon = nullptr;
  int source_id;
  int message_id;
  if (type[static_cast<size_t>(DeviceType::kCamera)]) {
    source_icon = &::vector_icons::kVideocamIcon;
    if (type[static_cast<size_t>(DeviceType::kMic)]) {
      source_id = IDS_CAMERA_MIC_NOTIFICATION_SOURCE;
      message_id = IDS_APP_USING_CAMERA_MIC_NOTIFICATION_MESSAGE;
    } else {
      source_id = IDS_CAMERA_NOTIFICATION_SOURCE;
      message_id = IDS_APP_USING_CAMERA_NOTIFICATION_MESSAGE;
    }
  } else {
    DCHECK_EQ(type, kMicNotification);
    source_icon = &::vector_icons::kMicIcon;
    source_id = IDS_MIC_NOTIFICATION_SOURCE;
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
      /*display_source=*/l10n_util::GetStringUTF16(source_id),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notifier_id),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          std::move(notification_observer_)));

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

void VmCameraMicManager::CloseNotification(VmType vm, NotificationType type) {
  DCHECK_NE(type, kNoNotification);
  if (!base::FeatureList::IsEnabled(
          features::kVmCameraMicIndicatorsAndNotifications)) {
    return;
  }
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationId(vm, type));
}

VmCameraMicManager::VmNotificationObserver::VmNotificationObserver(
    Profile* profile,
    OpenSettingsFunction open_settings)
    : profile_{profile}, open_settings_{open_settings} {}

VmCameraMicManager::VmNotificationObserver::~VmNotificationObserver() = default;

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

}  // namespace chromeos
