// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb/cros_usb_detector.h"

#include <fcntl.h>

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/arc_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr uint32_t kAllInterfacesMask = ~0U;
const char kParallelsShortName[] = "Parallels";

// Not owned locally.
static CrosUsbDetector* g_cros_usb_detector = nullptr;

const char kNotifierUsb[] = "crosusb.connected";

std::u16string ProductLabelFromDevice(
    const device::mojom::UsbDeviceInfo& device_info) {
  std::u16string product_label =
      l10n_util::GetStringUTF16(IDS_CROSUSB_UNKNOWN_DEVICE);
  if (device_info.product_name.has_value() &&
      !device_info.product_name->empty()) {
    product_label = device_info.product_name.value();
  } else if (device_info.manufacturer_name.has_value() &&
             !device_info.manufacturer_name->empty()) {
    product_label =
        l10n_util::GetStringFUTF16(IDS_CROSUSB_UNKNOWN_DEVICE_FROM_MANUFACTURER,
                                   device_info.manufacturer_name.value());
  }
  return product_label;
}

uint32_t ClearMatchingInterfaces(
    uint32_t in_mask,
    const device::mojom::UsbDeviceFilter& filter,
    const device::mojom::UsbDeviceInfo& device_info) {
  uint32_t mask = in_mask;

  for (auto& config : device_info.configurations) {
    for (auto& iface : config->interfaces) {
      for (auto& alternate_info : iface->alternates) {
        if (filter.has_class_code &&
            alternate_info->class_code != filter.class_code) {
          continue;
        }
        if (filter.has_subclass_code &&
            alternate_info->subclass_code != filter.subclass_code) {
          continue;
        }
        if (filter.has_protocol_code &&
            alternate_info->protocol_code != filter.protocol_code) {
          continue;
        }
        if (iface->interface_number >= 32) {
          LOG(ERROR) << "Interface number too high in USB descriptor";
          continue;
        }
        mask &= ~(1U << iface->interface_number);
      }
    }
  }

  return mask;
}

uint32_t GetUsbInterfaceBaseMask(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (device_info.configurations.empty()) {
    // No specific interfaces to clear.
    return kAllInterfacesMask;
  }
  uint32_t mask = 0;
  for (auto& config : device_info.configurations) {
    for (auto& iface : config->interfaces) {
      if (iface->interface_number >= 32) {
        LOG(ERROR) << "Interface number too high in USB descriptor.";
        continue;
      }
      mask |= (1U << iface->interface_number);
    }
  }
  return mask;
}

uint32_t GetFilteredInterfacesMask(
    const std::vector<device::mojom::UsbDeviceFilterPtr>& filters,
    const device::mojom::UsbDeviceInfo& device_info) {
  uint32_t mask = GetUsbInterfaceBaseMask(device_info);
  for (const auto& filter : filters) {
    mask = ClearMatchingInterfaces(mask, *filter, device_info);
  }
  return mask;
}

Profile* profile() {
  return ProfileManager::GetActiveUserProfile();
}

crostini::CrostiniManager* manager() {
  return crostini::CrostiniManager::GetForProfile(profile());
}

// Delegate for CrosUsb notification
class CrosUsbNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit CrosUsbNotificationDelegate(const std::string& notification_id,
                                       std::string guid,
                                       std::vector<std::string> vm_names,
                                       std::string settings_sub_page)
      : notification_id_(notification_id),
        guid_(std::move(guid)),
        vm_names_(std::move(vm_names)),
        settings_sub_page_(std::move(settings_sub_page)),
        disposition_(CrosUsbNotificationClosed::kUnknown) {}

  void Click(const base::Optional<int>& button_index,
             const base::Optional<std::u16string>& reply) override {
    disposition_ = CrosUsbNotificationClosed::kUnknown;
    if (button_index && *button_index < static_cast<int>(vm_names_.size())) {
      HandleConnectToVm(vm_names_[*button_index]);
    } else {
      HandleShowSettings(settings_sub_page_);
    }
  }

  void Close(bool by_user) override {
    if (by_user)
      disposition_ = chromeos::CrosUsbNotificationClosed::kByUser;
  }

 private:
  ~CrosUsbNotificationDelegate() override = default;
  void HandleConnectToVm(const std::string& vm_name) {
    disposition_ = CrosUsbNotificationClosed::kConnectToLinux;
    chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
    if (detector) {
      detector->AttachUsbDeviceToVm(vm_name, guid_, base::DoNothing());
      return;
    }
    Close(false);
  }

  void HandleShowSettings(const std::string& sub_page) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile(),
                                                                 sub_page);
    Close(false);
  }

  std::string notification_id_;
  std::string guid_;
  std::vector<std::string> vm_names_;
  std::string settings_sub_page_;
  CrosUsbNotificationClosed disposition_;
  base::WeakPtrFactory<CrosUsbNotificationDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosUsbNotificationDelegate);
};

// List of class codes to handle / not handle.
// See https://www.usb.org/defined-class-codes for more information.
enum UsbClassCode : uint8_t {
  USB_CLASS_PER_INTERFACE = 0x00,
  USB_CLASS_AUDIO = 0x01,
  USB_CLASS_COMM = 0x02,
  USB_CLASS_HID = 0x03,
  USB_CLASS_PHYSICAL = 0x05,
  USB_CLASS_STILL_IMAGE = 0x06,
  USB_CLASS_PRINTER = 0x07,
  USB_CLASS_MASS_STORAGE = 0x08,
  USB_CLASS_HUB = 0x09,
  USB_CLASS_CDC_DATA = 0x0a,
  USB_CLASS_CSCID = 0x0b,
  USB_CLASS_CONTENT_SEC = 0x0d,
  USB_CLASS_VIDEO = 0x0e,
  USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,
  USB_CLASS_BILLBOARD = 0x11,
  USB_CLASS_DIAGNOSTIC_DEVICE = 0xdc,
  USB_CLASS_WIRELESS_CONTROLLER = 0xe0,
  USB_CLASS_MISC = 0xef,
  USB_CLASS_APP_SPEC = 0xfe,
  USB_CLASS_VENDOR_SPEC = 0xff,
};

device::mojom::UsbDeviceFilterPtr UsbFilterByClassCode(
    UsbClassCode device_class) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = device_class;
  return filter;
}

void ShowNotificationForDevice(const std::string& guid,
                               const std::u16string& label) {
  message_center::RichNotificationData rich_notification_data;
  std::vector<std::string> vm_names;
  std::string settings_sub_page;
  std::u16string vm_name;
  rich_notification_data.small_image = gfx::Image(
      gfx::CreateVectorIcon(vector_icons::kUsbIcon, 64, gfx::kGoogleBlue800));
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile())) {
    vm_name = l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX);
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM, vm_name)));
    vm_names.emplace_back(crostini::kCrostiniDefaultVmName);
    settings_sub_page =
        chromeos::settings::mojom::kCrostiniUsbPreferencesSubpagePath;
  }
  if (plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile())) {
    vm_name = base::ASCIIToUTF16(kParallelsShortName);
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM, vm_name)));
    vm_names.emplace_back(plugin_vm::kPluginVmName);
    settings_sub_page =
        chromeos::settings::mojom::kPluginVmUsbPreferencesSubpagePath;
  }

  std::u16string message;
  if (vm_names.size() == 1) {
    message = l10n_util::GetStringFUTF16(
        IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION, label, vm_name);
  } else {
    // Note: we assume right now that multi-VM is Linux and Plugin VM.
    message = l10n_util::GetStringFUTF16(
        IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_LINUX_PLUGIN_VM, label);
    settings_sub_page = std::string();
  }

  std::string notification_id = CrosUsbDetector::MakeNotificationId(guid);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_TITLE),
      message, gfx::Image(), std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierUsb),
      rich_notification_data,
      base::MakeRefCounted<CrosUsbNotificationDelegate>(
          notification_id, guid, std::move(vm_names),
          std::move(settings_sub_page)));
  SystemNotificationHelper::GetInstance()->Display(notification);
}

class FilesystemUnmounter : public base::RefCounted<FilesystemUnmounter> {
 public:
  static void UnmountPaths(const std::set<std::string>& paths,
                           base::OnceCallback<void(bool success)> callback);

 private:
  friend class base::RefCounted<FilesystemUnmounter>;

  explicit FilesystemUnmounter(base::OnceCallback<void(bool success)> callback)
      : callback_(std::move(callback)) {}
  ~FilesystemUnmounter() { std::move(callback_).Run(success_); }

  void OnUnmountPath(MountError mount_error);

  bool success_ = true;
  base::OnceCallback<void(bool success)> callback_;
};

void FilesystemUnmounter::UnmountPaths(
    const std::set<std::string>& paths,
    base::OnceCallback<void(bool success)> callback) {
  scoped_refptr<FilesystemUnmounter> unmounter =
      new FilesystemUnmounter(std::move(callback));
  // When the last UnmountPath() calls completes, the ref count reaches zero
  // and the destructor fires the callback. We can't use base::BarrierClosure()
  // because we need to aggregate the MountError results.
  for (const std::string& path : paths) {
    disks::DiskMountManager::GetInstance()->UnmountPath(
        path, base::BindOnce(&FilesystemUnmounter::OnUnmountPath, unmounter));
  }
}

void FilesystemUnmounter::OnUnmountPath(MountError mount_error) {
  if (mount_error != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Error unmounting USB drive: " << mount_error;
    success_ = false;
  }
}

}  // namespace

CrosUsbDeviceInfo::CrosUsbDeviceInfo(std::string guid,
                                     std::u16string label,
                                     base::Optional<std::string> shared_vm_name,
                                     bool prompt_before_sharing)
    : guid(guid),
      label(label),
      shared_vm_name(shared_vm_name),
      prompt_before_sharing(prompt_before_sharing) {}
CrosUsbDeviceInfo::CrosUsbDeviceInfo(const CrosUsbDeviceInfo&) = default;
CrosUsbDeviceInfo::~CrosUsbDeviceInfo() = default;

std::string CrosUsbDetector::MakeNotificationId(const std::string& guid) {
  return "cros:" + guid;
}

// static
CrosUsbDetector* CrosUsbDetector::Get() {
  return g_cros_usb_detector;
}

CrosUsbDetector::CrosUsbDetector() {
  DCHECK(!g_cros_usb_detector);
  g_cros_usb_detector = this;
  guest_os_classes_blocked_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PHYSICAL));
  guest_os_classes_blocked_.emplace_back(UsbFilterByClassCode(USB_CLASS_HUB));
  guest_os_classes_blocked_.emplace_back(UsbFilterByClassCode(USB_CLASS_HID));
  guest_os_classes_blocked_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PRINTER));

  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_AUDIO));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_STILL_IMAGE));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_MASS_STORAGE));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_VIDEO));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_BILLBOARD));
  guest_os_classes_without_notif_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PERSONAL_HEALTHCARE));

  // If a device has an adb interface, we always allow it.
  const int kAdbSubclass = 0x42;
  const int kAdbProtocol = 0x1;
  adb_device_filter_ = UsbFilterByClassCode(USB_CLASS_VENDOR_SPEC);
  adb_device_filter_->has_subclass_code = true;
  adb_device_filter_->subclass_code = kAdbSubclass;
  adb_device_filter_->has_protocol_code = true;
  adb_device_filter_->protocol_code = kAdbProtocol;

  const int kFastbootProtocol = 0x3;
  fastboot_device_filter_ = UsbFilterByClassCode(USB_CLASS_VENDOR_SPEC);
  fastboot_device_filter_->has_subclass_code = true;
  fastboot_device_filter_->subclass_code = kAdbSubclass;
  fastboot_device_filter_->has_protocol_code = true;
  fastboot_device_filter_->protocol_code = kFastbootProtocol;

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->AddVmObserver(this);
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->AddObserver(this);
  disks::DiskMountManager::GetInstance()->AddObserver(this);
}

CrosUsbDetector::~CrosUsbDetector() {
  DCHECK_EQ(this, g_cros_usb_detector);
  disks::DiskMountManager::GetInstance()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetConciergeClient()->RemoveVmObserver(
      this);
  chromeos::DBusThreadManager::Get()
      ->GetVmPluginDispatcherClient()
      ->RemoveObserver(this);
  g_cros_usb_detector = nullptr;
}

void CrosUsbDetector::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager) {
  DCHECK(!device_manager_) << "device_manager_ was already initialized";
  device_manager_.Bind(std::move(device_manager));
}

void CrosUsbDetector::AddUsbDeviceObserver(CrosUsbDeviceObserver* observer) {
  usb_device_observers_.AddObserver(observer);
}

void CrosUsbDetector::RemoveUsbDeviceObserver(CrosUsbDeviceObserver* observer) {
  usb_device_observers_.RemoveObserver(observer);
}

void CrosUsbDetector::SignalUsbDeviceObservers() {
  for (auto& observer : usb_device_observers_) {
    observer.OnUsbDevicesChanged();
  }
}

std::vector<CrosUsbDeviceInfo> CrosUsbDetector::GetShareableDevices() const {
  std::vector<CrosUsbDeviceInfo> result;
  for (const auto& it : usb_devices_) {
    const UsbDevice& device = it.second;
    if (!device.shareable)
      continue;
    result.emplace_back(
        device.info->guid, device.label, device.shared_vm_name,
        /*prompt_before_sharing=*/
        device.shared_vm_name.has_value() || !device.mount_points.empty());
  }
  return result;
}

CrosUsbDetector::UsbDevice::UsbDevice() = default;
CrosUsbDetector::UsbDevice::UsbDevice(UsbDevice&&) = default;
CrosUsbDetector::UsbDevice::~UsbDevice() = default;

void CrosUsbDetector::ConnectToDeviceManager() {
  // Tests may set a fake manager.
  if (!device_manager_) {
    content::GetDeviceService().BindUsbDeviceManager(
        device_manager_.BindNewPipeAndPassReceiver());
  }
  DCHECK(device_manager_);
  device_manager_.set_disconnect_handler(
      base::BindOnce(&CrosUsbDetector::OnDeviceManagerConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  device_manager_->EnumerateDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&CrosUsbDetector::OnListAttachedDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool CrosUsbDetector::ShouldShowNotification(const UsbDevice& device) {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile()) &&
      !plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile())) {
    return false;
  }
  if (!device.shareable) {
    return false;
  }

  if (device::UsbDeviceFilterMatches(*adb_device_filter_, *device.info) ||
      device::UsbDeviceFilterMatches(*fastboot_device_filter_, *device.info)) {
    VLOG(1) << "Adb or fastboot device found";
    return true;
  }
  if ((GetFilteredInterfacesMask(guest_os_classes_without_notif_,
                                 *device.info) &
       device.allowed_interfaces_mask) != 0) {
    VLOG(1) << "At least one notifiable interface found for device";
    // Only notify if no interfaces were suppressed.
    return GetUsbInterfaceBaseMask(*device.info) ==
           device.allowed_interfaces_mask;
  }
  return false;
}

void CrosUsbDetector::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  ConnectSharedDevicesOnVmStartup(signal.name());
}

void CrosUsbDetector::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {}

void CrosUsbDetector::OnVmToolsStateChanged(
    const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal) {}

void CrosUsbDetector::OnVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  if (signal.vm_state() ==
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING) {
    ConnectSharedDevicesOnVmStartup(signal.vm_name());
  }
}

void CrosUsbDetector::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
    MountError error_code,
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  if (mount_info.mount_type != MOUNT_TYPE_DEVICE ||
      error_code != MOUNT_ERROR_NONE) {
    return;
  }

  const chromeos::disks::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(
          mount_info.source_path);

  // This can be null if a drive is physically removed.
  if (!disk) {
    return;
  }

  for (auto& it : usb_devices_) {
    UsbDevice& device = it.second;
    if (device.info->bus_number == disk->bus_number() &&
        device.info->port_number == disk->device_number()) {
      bool was_empty = device.mount_points.empty();
      if (event == disks::DiskMountManager::MOUNTING) {
        device.mount_points.insert(mount_info.mount_path);
      } else {
        device.mount_points.erase(mount_info.mount_path);
      }

      if (!device.is_unmounting && was_empty != device.mount_points.empty()) {
        SignalUsbDeviceObservers();
      }
      return;
    }
  }
}

void CrosUsbDetector::OnDeviceChecked(
    device::mojom::UsbDeviceInfoPtr device_info,
    bool hide_notification,
    bool allowed) {
  if (!allowed) {
    LOG(WARNING) << "Device not allowed by Permission Broker. product:"
                 << device_info->product_id
                 << " vendor:" << device_info->vendor_id;
    return;
  }

  UsbDevice new_device;

  new_device.label = ProductLabelFromDevice(*device_info);

  const bool has_supported_interface =
      device::UsbDeviceFilterMatches(*adb_device_filter_, *device_info) ||
      device::UsbDeviceFilterMatches(*fastboot_device_filter_, *device_info);

  new_device.allowed_interfaces_mask =
      GetFilteredInterfacesMask(guest_os_classes_blocked_, *device_info);

  new_device.shareable =
      has_supported_interface || new_device.allowed_interfaces_mask != 0;

  // Storage devices already plugged in at log-in time will already be mounted.
  for (const auto& iter : disks::DiskMountManager::GetInstance()->disks()) {
    if (iter.second->bus_number() == device_info->bus_number &&
        iter.second->device_number() == device_info->port_number &&
        iter.second->is_mounted()) {
      new_device.mount_points.insert(iter.second->mount_path());
    }
  }

  // Copy strings prior to moving |device_info| and |new_device|.
  std::string guid = device_info->guid;
  std::u16string label = new_device.label;

  new_device.info = std::move(device_info);
  auto result = usb_devices_.emplace(guid, std::move(new_device));

  if (!result.second) {
    LOG(ERROR) << "Ignoring USB device " << label << " as guid already exists.";
    return;
  }

  SignalUsbDeviceObservers();

  // Some devices should not trigger the notification.
  if (hide_notification || !ShouldShowNotification(result.first->second)) {
    VLOG(1) << "Not showing USB notification for " << label;
    return;
  }

  ShowNotificationForDevice(guid, label);
}

void CrosUsbDetector::OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) {
  CrosUsbDetector::OnDeviceAdded(std::move(device), false);
}

void CrosUsbDetector::OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info,
                                    bool hide_notification) {
  std::string guid = device_info->guid;
  device_manager_->CheckAccess(
      guid, base::BindOnce(&CrosUsbDetector::OnDeviceChecked,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(device_info), hide_notification));
}

void CrosUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(device_info->guid));

  std::string guid = device_info->guid;
  auto it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(ERROR) << "Unknown USB device removed: "
               << ProductLabelFromDevice(*device_info);
    return;
  }

  if (it->second.shared_vm_name) {
    DetachUsbDeviceFromVm(*it->second.shared_vm_name, guid, base::DoNothing());
  }
  usb_devices_.erase(it);
  SignalUsbDeviceObservers();
}

void CrosUsbDetector::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_receiver_.reset();
  ConnectToDeviceManager();
}

void CrosUsbDetector::ConnectSharedDevicesOnVmStartup(
    const std::string& vm_name) {
  // Reattach shared devices when the VM becomes available.
  for (auto& it : usb_devices_) {
    auto& device = it.second;
    if (device.shared_vm_name == vm_name) {
      VLOG(1) << "Connecting " << device.label << " to " << vm_name;
      // Clear any older guest_port setting.
      device.guest_port = base::nullopt;
      AttachUsbDeviceToVm(vm_name, device.info->guid, base::DoNothing());
    }
  }
}

void CrosUsbDetector::AttachUsbDeviceToVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  const auto& it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(WARNING) << "Attempted to attach device that does not exist: " << guid;
    std::move(callback).Run(false);
    return;
  }

  if (!it->second.shareable) {
    LOG(ERROR) << "Attempted to attach non-shareable device: "
               << it->second.label;
    std::move(callback).Run(false);
    return;
  }

  // If we tried to share a device to a VM that wasn't started, |shared_vm_name|
  // would be set but |guest_port| would be empty. Once the VM is started, we go
  // through this flow again.
  if (it->second.shared_vm_name == vm_name &&
      it->second.guest_port.has_value()) {
    VLOG(1) << "Device " << it->second.label << " is already shared with vm "
            << vm_name;
    std::move(callback).Run(true);
    return;
  }

  UnmountFilesystems(vm_name, guid, std::move(callback));
}

void CrosUsbDetector::DetachUsbDeviceFromVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  const auto& it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(WARNING) << "Attempted to detach device that does not exist: " << guid;
    std::move(callback).Run(/*success=*/true);
    return;
  }

  UsbDevice& device = it->second;
  if (device.shared_vm_name != vm_name) {
    LOG(WARNING) << "Failed to detach " << guid << " from " << vm_name
                 << ". It appears to be shared with "
                 << (device.shared_vm_name ? *device.shared_vm_name
                                           : "[not shared]")
                 << " at port "
                 << (device.guest_port
                         ? base::NumberToString(*device.guest_port)
                         : "[not attached]")
                 << ".";

    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (!device.guest_port) {
    // The VM hasn't been started yet, attaching is in progress, or attaching
    // failed.
    // TODO(timloh): Check what happens if attaching to a different VM races
    // with an in progress attach.
    RelinquishDeviceClaim(guid);
    device.shared_vm_name = base::nullopt;
    SignalUsbDeviceObservers();
    std::move(callback).Run(/*success=*/true);
    return;
  }

  vm_tools::concierge::DetachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_guest_port(*device.guest_port);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->DetachUsbDevice(
      std::move(request),
      base::BindOnce(&CrosUsbDetector::OnUsbDeviceDetachFinished,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnListAttachedDevices(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (device::mojom::UsbDeviceInfoPtr& device_info : devices)
    CrosUsbDetector::OnDeviceAdded(std::move(device_info),
                                   /*hide_notification*/ true);
}

void CrosUsbDetector::UnmountFilesystems(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  auto it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(ERROR) << "Couldn't find device " << guid;
    std::move(callback).Run(false);
    return;
  }

  it->second.is_unmounting = true;
  FilesystemUnmounter::UnmountPaths(
      it->second.mount_points,
      base::BindOnce(&CrosUsbDetector::OnUnmountFilesystems,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnUnmountFilesystems(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    bool unmount_success) {
  auto it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(ERROR) << "Couldn't find device " << guid;
    std::move(callback).Run(false);
    return;
  }

  UsbDevice& device = it->second;
  device.is_unmounting = false;

  if (!unmount_success) {
    // FilesystemUnmounter already logged the error.
    std::move(callback).Run(false);
    return;
  }

  // Detach first if device is attached elsewhere
  if (device.shared_vm_name && device.shared_vm_name != vm_name) {
    DetachUsbDeviceFromVm(
        *device.shared_vm_name, guid,
        base::BindOnce(&CrosUsbDetector::AttachAfterDetach,
                       weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                       device.allowed_interfaces_mask, std::move(callback)));
  } else {
    // The device isn't attached.
    AttachAfterDetach(vm_name, guid, device.allowed_interfaces_mask,
                      std::move(callback),
                      /*detach_success=*/true);
  }
}

void CrosUsbDetector::AttachAfterDetach(
    const std::string& vm_name,
    const std::string& guid,
    uint32_t allowed_interfaces_mask,
    base::OnceCallback<void(bool success)> callback,
    bool detach_success) {
  if (!detach_success) {
    LOG(ERROR) << "Failed to detatch before attach";
    std::move(callback).Run(false);
    return;
  }

  auto it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(ERROR) << "No device info for " << guid;
    std::move(callback).Run(false);
    return;
  }

  auto& device = it->second;

  // Mark the USB device shared so that it will be shared when the VM starts
  // if it isn't started yet. This also ensures the UI will show the device as
  // shared. The guest_port will be set later.
  device.shared_vm_name = vm_name;

  auto claim_it = devices_claimed_.find(guid);
  if (claim_it != devices_claimed_.end()) {
    if (claim_it->second.device_file.IsValid()) {
      // We take a dup here which will be closed if DoVmAttach fails.
      base::ScopedFD device_fd(
          claim_it->second.device_file.Duplicate().TakePlatformFile());
      DoVmAttach(vm_name, device.info.Clone(), std::move(device_fd),
                 std::move(callback));
    } else {
      LOG(WARNING) << "Device " << guid << " already claimed and awaiting fd.";
      std::move(callback).Run(false);
    }
    return;
  }

  VLOG(1) << "Opening " << guid << " with mask " << std::hex
          << allowed_interfaces_mask;

  base::ScopedFD read_end, write_end;
  if (!base::CreatePipe(&read_end, &write_end, /*non_blocking=*/true)) {
    LOG(ERROR) << "Couldn't create pipe for " << guid;
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "Saving lifeline_fd " << write_end.get();
  devices_claimed_[guid].lifeline_file = base::File(std::move(write_end));

  // Open a file descriptor to pass to CrostiniManager & Concierge.
  device_manager_->OpenFileDescriptor(
      guid, allowed_interfaces_mask, mojo::PlatformHandle(std::move(read_end)),
      base::BindOnce(&CrosUsbDetector::OnAttachUsbDeviceOpened,
                     weak_ptr_factory_.GetWeakPtr(), vm_name,
                     device.info.Clone(), std::move(callback)));

  // Close any associated notifications (the user isn't using them). This
  // destroys the CrosUsbNotificationDelegate and vm_name and guid args may be
  // invalid after Close.
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(guid));
}

void CrosUsbDetector::OnAttachUsbDeviceOpened(
    const std::string& vm_name,
    device::mojom::UsbDeviceInfoPtr device_info,
    base::OnceCallback<void(bool success)> callback,
    base::File file) {
  if (!file.IsValid()) {
    LOG(ERROR) << "Permission broker refused access to USB device";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  devices_claimed_[device_info->guid].device_file = file.Duplicate();
  if (!manager()) {
    LOG(ERROR) << "Attaching device without Crostini manager instance";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  DoVmAttach(vm_name, device_info.Clone(),
             base::ScopedFD(file.TakePlatformFile()), std::move(callback));
}

void CrosUsbDetector::DoVmAttach(
    const std::string& vm_name,
    device::mojom::UsbDeviceInfoPtr device_info,
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> callback) {
  vm_tools::concierge::AttachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_bus_number(device_info->bus_number);
  request.set_port_number(device_info->port_number);
  request.set_vendor_id(device_info->vendor_id);
  request.set_product_id(device_info->product_id);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->AttachUsbDevice(
      std::move(fd), std::move(request),
      base::BindOnce(&CrosUsbDetector::OnUsbDeviceAttachFinished,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, device_info->guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnUsbDeviceAttachFinished(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    base::Optional<vm_tools::concierge::AttachUsbDeviceResponse> response) {
  bool success = true;
  if (!response) {
    LOG(ERROR) << "Failed to attach USB device, empty dbus response";
    success = false;
  } else if (!response->success()) {
    LOG(ERROR) << "Failed to attach USB device, " << response->reason();
    success = false;
  }

  if (success) {
    auto it = usb_devices_.find(guid);
    if (it == usb_devices_.end()) {
      LOG(WARNING) << "Dbus response indicates successful attach but device "
                   << "info was missing for " << guid;
      success = false;
    } else {
      it->second.shared_vm_name = vm_name;
      it->second.guest_port = response->guest_port();
    }
  }
  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::OnUsbDeviceDetachFinished(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    base::Optional<vm_tools::concierge::DetachUsbDeviceResponse> response) {
  bool success = true;
  if (!response) {
    LOG(ERROR) << "Failed to detach USB device, empty dbus response";
    success = false;
  } else if (!response->success()) {
    LOG(ERROR) << "Failed to detach USB device, " << response->reason();
    success = false;
  }

  auto it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(WARNING) << "Dbus response indicates successful detach but device info "
                 << "was missing for " << guid;
  } else {
    it->second.shared_vm_name = base::nullopt;
    it->second.guest_port = base::nullopt;
  }
  RelinquishDeviceClaim(guid);
  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::RelinquishDeviceClaim(const std::string& guid) {
  auto it = devices_claimed_.find(guid);
  if (it != devices_claimed_.end()) {
    VLOG(1) << "Closing lifeline_fd "
            << it->second.lifeline_file.GetPlatformFile();
    devices_claimed_.erase(it);
  } else {
    LOG(ERROR) << "Relinquishing device with no prior claim: " << guid;
  }
}

}  // namespace chromeos
