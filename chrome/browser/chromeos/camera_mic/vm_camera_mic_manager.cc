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

std::string GetNotificationId(VmCameraMicManager::VmType vm,
                              VmCameraMicManager::DeviceType device) {
  std::string id = kNotificationIdPrefix;

  switch (vm) {
    case VmCameraMicManager::VmType::kCrostiniVm:
      id.append("-crostini");
      break;
    case VmCameraMicManager::VmType::kPluginVm:
      id.append("-pluginvm");
      break;
  }

  switch (device) {
    case VmCameraMicManager::DeviceType::kCamera:
      id.append("-camera");
      break;
    case VmCameraMicManager::DeviceType::kMic:
      id.append("-mic");
      break;
  }

  return id;
}

}  // namespace

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
    for (DeviceType device : {DeviceType::kMic, DeviceType::kCamera}) {
      active_map_[std::make_pair(vm, device)] = false;
    }
  }
}

VmCameraMicManager::~VmCameraMicManager() = default;

void VmCameraMicManager::SetActive(VmType vm, DeviceType device, bool active) {
  auto active_it = active_map_.find(std::make_pair(vm, device));
  CHECK(active_it != active_map_.end());
  if (active_it->second != active) {
    if (!observer_timer_.IsRunning()) {
      observer_timer_.Reset();
    }
    active_it->second = active;
    if (active) {
      OpenNotification(vm, device);
    } else {
      CloseNotification(vm, device);
    }
  }
}

bool VmCameraMicManager::GetActive(VmType vm, DeviceType device) const {
  auto active_it = active_map_.find(std::make_pair(vm, device));
  CHECK(active_it != active_map_.end());
  return active_it->second;
}

bool VmCameraMicManager::GetDeviceActive(DeviceType device) const {
  for (auto& type_active : active_map_) {
    if (type_active.first.second == device && type_active.second) {
      return true;
    }
  }
  return false;
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

void VmCameraMicManager::OpenNotification(VmType vm, DeviceType device) {
  if (!base::FeatureList::IsEnabled(
          features::kVmCameraMicIndicatorsAndNotifications)) {
    return;
  }

  int source_id;
  const gfx::VectorIcon* source_icon = nullptr;
  const char* notifier_id = nullptr;
  int message_id;
  switch (device) {
    case VmCameraMicManager::DeviceType::kCamera:
      source_id = IDS_CAMERA_NOTIFICATION_SOURCE;
      source_icon = &::vector_icons::kVideocamIcon;
      notifier_id = ash::kVmCameraNotifierId;
      message_id = IDS_APP_USING_CAMERA_NOTIFICATION_MESSAGE;
      break;
    case VmCameraMicManager::DeviceType::kMic:
      source_id = IDS_MIC_NOTIFICATION_SOURCE;
      source_icon = &::vector_icons::kMicIcon;
      notifier_id = ash::kVmMicNotifierId;
      message_id = IDS_APP_USING_MIC_NOTIFICATION_MESSAGE;
      break;
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

  // TODO(b/167491603): check if NotificationPriority should be higher than
  // default.
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = source_icon;
  rich_notification_data.pinned = true;
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationId(vm, device),
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

void VmCameraMicManager::CloseNotification(VmType vm, DeviceType device) {
  if (!base::FeatureList::IsEnabled(
          features::kVmCameraMicIndicatorsAndNotifications)) {
    return;
  }
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationId(vm, device));
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
