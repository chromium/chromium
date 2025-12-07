// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"

#include <string>
#include <tuple>
#include <utility>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/video_conference/video_conference_ash_feature_client.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
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

namespace ash {

namespace {

const char kNotificationIdPrefix[] = "vm_camera_mic_manager";

}  // namespace

constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kNoNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kMicNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kCameraNotification;
constexpr VmCameraMicManager::NotificationType
    VmCameraMicManager::kCameraAndMicNotification;
constexpr base::TimeDelta VmCameraMicManager::kDebounceTime;

// VmInfo stores the camera/mic information for a VM. It also controls the
// notifications for the VM. We either do not display a notification at all, or
// display a single notification, which can be a "camera", "mic", or a "camera
// and mic" notification.
//
// Some apps will quickly turn on and off devices multiple times (e.g. skype in
// Parallels does this about 5 times when starting a meeting). To avoid flashing
// multiple notifications, we implement a debounce algorithm here. The debounce
// algorithm needs to handle the following situations:
//
// * when a VM opens the camera and mic subsequently with a small delay
//   in-between, we should only show the "camera and mic" notification, instead
//   of showing the "camera" notification first and then switching to the
//   "camera and mic" one.
// * when a VM turns on a device and then immediately turns it off (e.g. taking
//   a photo), we should make sure the notification is shown (for a short period
//   of time). So, the debounce algorithm should not naively accumulate device
//   changes and then only act on the final accumulated state.
//
//
// How the debounce algorithm works
// ================================
//
// Basically, when a new device update comes, the algorithm starts a debounce
// period for `kDebounceTime`, during which we record the changes, and we update
// the notification one or more times afterwards.
//
// The type of notification is represented by `NotificationType`, which is a
// bitset of two bits. For a "camera" notification, only the camera bit is set
// (i.e. `10`). A "camera and mic" notification sets both bits (i.e. `11`). If
// no notification should be shown, we set both bits to 0 (i.e. `00`).
//
// Our algorithm maintains 3 `NotificationType` variables (see
// `notifications_`):
//
// * active: this is what is currently displaying. When we say setting `active`
//           to some value, we also mean updating the displaying notification.
// * target: this is updated immediately whenever device updates come in, so it
//           represents the latest state. If a device is turned on and off
//           immediately, obviously the effect is erased from target. This is
//           why we need another variable `stage`.
// * stage: this is updated immediately whenever a device is turned *on*.
//          Turning off a device does not affect this directly.
//
// This algorithm for updating `target` and `stage` is implemented in
// `OnDeviceUpdated()`, which also starts/stops the debounce timer if necessary.
//
// When `active == stage == target`, we are "stable" --- we don't need to do
// anything (until the next device update). And
// `SyncNotificationAndIndicators()` is normally what brings us to stable. It is
// called when the timer expired. This is what it does:
//
// * If `active != stage`, we set `active = stage`. Timer is reset if we are
//   still not stable.
// * Otherwise, `active == stage != target`. we set `active = stage = target`.
//   We reach the stable state now.
//
// Here is an example where the mic is turned on, the camera is turned on and
// then off immediately, and mic is turned off at the end after some time. We
// denote the state of the system with 6 bits: <active>-<stage>-<target>.
//
// 1: 00-00-00  # Stable, nothing is on.
// 2: 00-01-01  # Mic turning on, debounce timer is started.
// 3: 00-11-11  # Camera turning on, still in debounce period.
// 4: 00-11-01  # Camera turning off, still in debounce period.
// 5: 11-11-01  # Timer expired. `SyncNotificationAndIndicators()` sets
//              # `active=stage` (shows "camera and mic" notification). Reset
//              # the timer.
// 6: 01-01-01  # Timer expired. `SyncNotificationAndIndicators()` sets
//              # `active=stage=target` (shows mic notification).  We are stable
//              # now.
// 7: 01-01-00  # Mic turning off, debounce timer is started.
// 8: 00-00-00  # Timer expired. Same as 6, but no notification is shown now.
//              # Reach stable again.
class VmCameraMicManager::VmInfo : public message_center::NotificationObserver {
 public:
  VmInfo(Profile* profile,
         VmType vm_type,
         int name_id,
         base::RepeatingClosure on_notification_changed)
      : profile_(profile),
        vm_type_(vm_type),
        name_id_(name_id),
        notification_changed_callback_(on_notification_changed),
        debounce_timer_(
            FROM_HERE,
            kDebounceTime,
            base::BindRepeating(&VmInfo::SyncNotificationAndIndicators,
                                // Unretained because the timer
                                // cannot outlive the parent.
                                base::Unretained(this))) {}
  ~VmInfo() = default;

  VmType vm_type() const { return vm_type_; }
  int name_id() const { return name_id_; }
  NotificationType notification_type() const { return notifications_.active; }

  void SetMicActive(bool active) {
    OnDeviceUpdated(DeviceType::kMic, active);

    if (features::IsVideoConferenceEnabled()) {
      VideoConferenceAshFeatureClient* vc_ash_feature_client =
          VideoConferenceAshFeatureClient::Get();
      // Only calls `OnVmDeviceUpdated()` if `VideoConferenceAshFeatureClient`
      // has initialized, otherwise it will be handled at
      // `VideoConferenceAshFeatureClient` initialization.
      if (vc_ash_feature_client) {
        vc_ash_feature_client->OnVmDeviceUpdated(vm_type_, DeviceType::kMic,
                                                 active);
      }
    }
  }

  void SetCameraAccessing(bool accessing) {
    camera_accessing_ = accessing;
    OnCameraUpdated();

    if (features::IsVideoConferenceEnabled()) {
      VideoConferenceAshFeatureClient* vc_ash_feature_client =
          VideoConferenceAshFeatureClient::Get();
      // Only calls `OnVmDeviceUpdated()` if `VideoConferenceAshFeatureClient`
      // has initialized, otherwise it will be handled at
      // `VideoConferenceAshFeatureClient` initialization.
      if (vc_ash_feature_client) {
        vc_ash_feature_client->OnVmDeviceUpdated(vm_type_, DeviceType::kCamera,
                                                 accessing);
      }
    }
  }
  void SetCameraPrivacyIsOn(bool on) {
    camera_privacy_is_on_ = on;
    OnCameraUpdated();
  }

 private:
  void OnCameraUpdated() {
    OnDeviceUpdated(DeviceType::kCamera,
                    camera_accessing_ && !camera_privacy_is_on_);
  }

  // See document at the beginning of class.
  void OnDeviceUpdated(DeviceType device, bool value) {
    size_t device_index = static_cast<size_t>(device);

    notifications_.target.set(device_index, value);
    if (value) {
      notifications_.stage.set(device_index, value);
    }

    VLOG(1) << "update stage/target vm_type=" << static_cast<int>(vm_type_)
            << " state: " << notifications_.active << "-"
            << notifications_.stage << "-" << notifications_.target;

    SyncTimer();
  }

  void SyncTimer() {
    const bool stable = notifications_.active == notifications_.stage &&
                        notifications_.active == notifications_.target;
    const bool should_run_timer = !stable;
    const bool is_running = debounce_timer_.IsRunning();

    if (should_run_timer && !is_running) {
      debounce_timer_.Reset();
    } else if (!should_run_timer && is_running) {
      debounce_timer_.Stop();
    }
  }

  void UpdateActiveNotificationAndIndicators(
      NotificationType new_notification) {
    DCHECK_NE(notifications_.active, new_notification);

    auto app_name = l10n_util::GetStringUTF16(name_id_);
    auto delegate = base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(
        /*launch_settings=*/base::BindRepeating(
            &VmCameraMicManager::VmInfo::OpenSettings,
            weak_ptr_factory_.GetWeakPtr()));

    // Privacy indicators is only enabled when Video Conference is disabled.
    bool privacy_indicators_enabled = !features::IsVideoConferenceEnabled();

    if (notifications_.active != kNoNotification) {
      if (privacy_indicators_enabled) {
        PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
            /*app_id=*/GetNotificationId(vm_type_, notifications_.active),
            app_name, /*is_camera_used=*/false, /*is_microphone_used=*/false,
            delegate, PrivacyIndicatorsSource::kLinuxVm);
      } else {
        CloseNotification(notifications_.active);
      }
    }

    if (new_notification != kNoNotification) {
      // Privacy indicator is only enabled when Video Conference is disabled.
      if (privacy_indicators_enabled) {
        PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
            /*app_id=*/GetNotificationId(vm_type_, new_notification), app_name,
            /*is_camera_used=*/
            new_notification[static_cast<size_t>(DeviceType::kCamera)],
            /*is_microphone_used=*/
            new_notification[static_cast<size_t>(DeviceType::kMic)], delegate,
            PrivacyIndicatorsSource::kLinuxVm);
      } else {
        OpenNotification(new_notification);
      }
    }

    notifications_.active = new_notification;
    notification_changed_callback_.Run();
  }

  // See document at the beginning of class.
  void SyncNotificationAndIndicators() {
    if (notifications_.active != notifications_.stage) {
      UpdateActiveNotificationAndIndicators(notifications_.stage);
      SyncTimer();

      VLOG(1) << "sync from stage. vm_type=" << static_cast<int>(vm_type_)
              << " state: " << notifications_.active << "-"
              << notifications_.stage << "-" << notifications_.target;
      return;
    }

    // Only target notification is different.
    DCHECK_NE(notifications_.active, notifications_.target);
    notifications_.stage = notifications_.target;
    UpdateActiveNotificationAndIndicators(notifications_.target);
    VLOG(1) << "sync from target. vm_type=" << static_cast<int>(vm_type_)
            << " state: " << notifications_.active << "-"
            << notifications_.stage << "-" << notifications_.target;
    // No need to call `SyncTimer()` because we have reached the stable state
    // here.
  }

  void OpenNotification(NotificationType type) const {
    CHECK(features::IsVideoConferenceEnabled());
    CHECK_NE(type, kNoNotification);

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

    message_center::RichNotificationData rich_notification_data;
    rich_notification_data.vector_small_image = source_icon;
    rich_notification_data.pinned = true;
    rich_notification_data.buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS));
    rich_notification_data.fullscreen_visibility =
        message_center::FullscreenVisibility::OVER_USER;

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GetNotificationId(vm_type_, type),
        /*title=*/
        l10n_util::GetStringFUTF16(message_id,
                                   l10n_util::GetStringUTF16(name_id_)),
        /*message=*/std::u16string(),
        /*icon=*/ui::ImageModel(),
        /*display_source=*/
        l10n_util::GetStringUTF16(IDS_CHROME_OS_NOTIFICATION_SOURCE),
        /*origin_url=*/GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT,
            kVmCameraMicNotifierId, NotificationCatalogName::kVMCameraMic),
        rich_notification_data,
        base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
            weak_ptr_factory_.GetMutableWeakPtr()));

    NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
        NotificationHandler::Type::TRANSIENT, notification,
        /*metadata=*/nullptr);
  }

  void CloseNotification(NotificationType type) const {
    CHECK(features::IsVideoConferenceEnabled());
    CHECK_NE(type, kNoNotification);

    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT,
        GetNotificationId(vm_type_, type));
  }

  // message_center::NotificationObserver:
  //
  // This open the settings page if the button is clicked on the notification.
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    OpenSettings();
  }

  // Opens the settings page.
  void OpenSettings() const {
    switch (vm_type_) {
      case VmType::kCrostiniVm:
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            profile_, chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
        break;
      case VmType::kPluginVm:
        chrome::ShowAppManagementPage(
            profile_, plugin_vm::kPluginVmShelfAppId,
            settings::AppManagementEntryPoint::kNotificationPluginVm);
        break;
      case VmType::kBorealis:
        chrome::ShowAppManagementPage(
            profile_, borealis::kClientAppId,
            settings::AppManagementEntryPoint::kAppManagementMainViewBorealis);
        break;
    }
  }

  const raw_ptr<Profile, LeakedDanglingUntriaged> profile_;
  const VmType vm_type_;
  const int name_id_;
  base::RepeatingClosure notification_changed_callback_;

  bool camera_accessing_ = false;
  // We don't actually need to store this separately for each VM, but this
  // makes code simpler.
  bool camera_privacy_is_on_ = false;

  // See document at the beginning of class.
  struct {
    NotificationType active;
    NotificationType stage;
    NotificationType target;
  } notifications_;

  base::RetainingOneShotTimer debounce_timer_;

  base::WeakPtrFactory<VmInfo> weak_ptr_factory_{this};
};

VmCameraMicManager* VmCameraMicManager::Get() {
  static base::NoDestructor<VmCameraMicManager> manager;
  return manager.get();
}

VmCameraMicManager::VmCameraMicManager() = default;

void VmCameraMicManager::OnPrimaryUserSessionStarted(Profile* primary_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  primary_profile_ = primary_profile;

  auto emplace_vm_info = [this](VmType vm, int name_id) {
    vm_info_map_.emplace(
        std::piecewise_construct, std::forward_as_tuple(vm),
        std::forward_as_tuple(
            primary_profile_, vm, name_id,
            base::BindRepeating(&VmCameraMicManager::NotifyActiveChanged,
                                base::Unretained(this))));
  };

  emplace_vm_info(VmType::kCrostiniVm, IDS_CROSTINI_LINUX);
  emplace_vm_info(VmType::kPluginVm, IDS_PLUGIN_VM_APP_NAME);
  emplace_vm_info(VmType::kBorealis, IDS_BOREALIS_APP_NAME);

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

// The class is supposed to be used as a singleton with `base::NoDestructor`,
// so we do not do clean up (e.g. deregister as observers) here.
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
  auto privacy_switch_state = cros::mojom::CameraPrivacySwitchState::UNKNOWN;
  auto device_id_to_privacy_switch_state =
      camera->AddCameraPrivacySwitchObserver(this);
  // TODO(b/255249223): Handle multiple cameras with privacy controls
  // properly.
  for (const auto& it : device_id_to_privacy_switch_state) {
    cros::mojom::CameraPrivacySwitchState state = it.second;
    if (state == cros::mojom::CameraPrivacySwitchState::ON) {
      privacy_switch_state = state;
      break;
    } else if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
      privacy_switch_state = state;
    }
  }
  OnCameraHWPrivacySwitchStateChanged(
      /*device_id=*/std::string(), privacy_switch_state);
}

void VmCameraMicManager::UpdateVmInfo(VmType vm,
                                      void (VmInfo::*updator)(bool),
                                      bool value) {
  auto it = vm_info_map_.find(vm);
  CHECK(it != vm_info_map_.end());
  auto& vm_info = it->second;

  (vm_info.*updator)(value);
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

bool VmCameraMicManager::IsDeviceActive(VmType vm, DeviceType device) const {
  auto it = vm_info_map_.find(vm);
  if (it == vm_info_map_.end()) {
    return false;
  }
  const NotificationType& notification_type = it->second.notification_type();
  if (notification_type[static_cast<size_t>(device)]) {
    return true;
  }
  return false;
}

bool VmCameraMicManager::IsNotificationActive(
    NotificationType notification) const {
  for (const auto& vm_info : vm_info_map_) {
    if (vm_info.second.notification_type() == notification) {
      return true;
    }
  }
  return false;
}

void VmCameraMicManager::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_new_active_client,
    const base::flat_set<std::string>& active_device_ids) {
  // Crostini does not support camera yet.
  bool client_active_state_changed =
      is_new_active_client || active_device_ids.empty();

  if (client_active_state_changed &&
      type == cros::mojom::CameraClientType::PLUGINVM) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&VmCameraMicManager::SetCameraAccessing,
                                  base::Unretained(this), VmType::kPluginVm,
                                  !active_device_ids.empty()));
  }
}

void VmCameraMicManager::SetCameraAccessing(VmType vm, bool accessing) {
  UpdateVmInfo(vm, &VmInfo::SetCameraAccessing, accessing);
}

void VmCameraMicManager::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&VmCameraMicManager::SetCameraPrivacyIsOn,
                                base::Unretained(this), is_on));
}

void VmCameraMicManager::SetCameraPrivacyIsOn(bool is_on) {
  DCHECK(!vm_info_map_.empty());
  for (auto& vm_and_info : vm_info_map_) {
    UpdateVmInfo(/*vm=*/vm_and_info.first, &VmInfo::SetCameraPrivacyIsOn,
                 is_on);
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
    case VmType::kBorealis:
      id.append("-borealis");
      break;
  }

  id.append(type.to_string());

  return id;
}

void VmCameraMicManager::OnNumberOfInputStreamsWithPermissionChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& clients_and_numbers =
      CrasAudioHandler::Get()->GetNumberOfInputStreamsWithPermission();

  auto update = [&](CrasAudioHandler::ClientType cras_client_type, VmType vm) {
    auto it = clients_and_numbers.find(cras_client_type);
    bool active = (it != clients_and_numbers.end() && it->second != 0);

    SetMicActive(vm, active);
  };

  update(CrasAudioHandler::ClientType::VM_TERMINA, VmType::kCrostiniVm);
  update(CrasAudioHandler::ClientType::VM_PLUGIN, VmType::kPluginVm);
  update(CrasAudioHandler::ClientType::VM_BOREALIS, VmType::kBorealis);
}

void VmCameraMicManager::SetMicActive(VmType vm, bool active) {
  UpdateVmInfo(vm, &VmInfo::SetMicActive, active);
}

}  // namespace ash
