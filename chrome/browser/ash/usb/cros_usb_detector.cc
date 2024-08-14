// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/usb/cros_usb_detector.h"

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {

namespace {

constexpr uint32_t kAllInterfacesMask = ~0U;
const char16_t kParallelsShortName[] = u"Parallels";
const char16_t kParallelsName[] = u"Parallels Desktop";

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

  CrosUsbNotificationDelegate(const CrosUsbNotificationDelegate&) = delete;
  CrosUsbNotificationDelegate& operator=(const CrosUsbNotificationDelegate&) =
      delete;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    disposition_ = CrosUsbNotificationClosed::kUnknown;
    if (button_index && *button_index < static_cast<int>(vm_names_.size())) {
      if (vm_names_[*button_index] == crostini::kCrostiniDefaultVmName) {
        // When multi-container is enabled, show the settings page instead of
        // directly attaching the device to the VM. Otherwise, the device is
        // attached to the default container in the VM.
        if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(
                profile())) {
          HandleShowSettings(
              chromeos::settings::mojom::kCrostiniUsbPreferencesSubpagePath);
        } else {
          HandleConnectToGuest(crostini::DefaultContainerId());
        }
      } else {
        HandleConnectToGuest(vm_names_[*button_index]);
      }
    } else {
      HandleShowSettings(settings_sub_page_);
    }
  }

  void Close(bool by_user) override {
    if (by_user) {
      disposition_ = CrosUsbNotificationClosed::kByUser;
    }
  }

 private:
  ~CrosUsbNotificationDelegate() override = default;
  void HandleConnectToGuest(const guest_os::GuestId& guest_id) {
    disposition_ = CrosUsbNotificationClosed::kConnectToLinux;
    CrosUsbDetector* detector = CrosUsbDetector::Get();
    if (detector) {
      detector->AttachUsbDeviceToGuest(guest_id, guid_, base::DoNothing());
      return;
    }
    Close(false);
  }

  void HandleConnectToGuest(const std::string& vm_name) {
    HandleConnectToGuest(guest_os::GuestId(vm_name, ""));
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
};

device::mojom::UsbDeviceFilterPtr UsbFilterByClassCode(
    UsbClassCode device_class) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = device_class;
  return filter;
}

device::mojom::UsbDeviceFilterPtr UsbFilterByClassAndSubclassCode(
    UsbClassCode device_class,
    UsbSubclassCode device_subclass) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = device_class;
  filter->has_subclass_code = true;
  filter->subclass_code = device_subclass;
  return filter;
}

std::u16string CombineVmNames(const std::vector<std::u16string>& vm_names) {
  std::u16string res;
  size_t pos = 0;
  while (pos < vm_names.size()) {
    res.append(vm_names[pos]);
    pos++;
    if (pos < vm_names.size()) {
      res.append(u" ");
      res.append(l10n_util::GetStringUTF16(IDS_CROSUSB_NOTIFICATION_OR));
      res.append(u" ");
    }
  }
  return res;
}

// Returns true if user enables ARC on ARCVM enabled devices.
bool IsPlayStoreEnabledWithArcVmForProfile(const Profile* profile) {
  return arc::IsArcPlayStoreEnabledForProfile(profile) && arc::IsArcVmEnabled();
}

void ShowNotificationForDevice(const std::string& guid,
                               const std::u16string& label) {
  message_center::RichNotificationData rich_notification_data;
  std::vector<std::string> vm_names;
  std::string settings_sub_page;
  std::u16string vm_name;
  std::u16string vm_name_button_text;
  std::vector<std::u16string> vm_names_in_notification;
  rich_notification_data.small_image = gfx::Image(
      gfx::CreateVectorIcon(vector_icons::kUsbIcon, 64, gfx::kGoogleBlue800));

  rich_notification_data.accent_color_id = cros_tokens::kCrosSysPrimary;

  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile())) {
    vm_name = l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX);
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM, vm_name)));
    vm_names.emplace_back(crostini::kCrostiniDefaultVmName);
    vm_names_in_notification.emplace_back(vm_name);
    settings_sub_page =
        chromeos::settings::mojom::kCrostiniUsbPreferencesSubpagePath;
  }
  if (plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile())) {
    vm_name = kParallelsName;
    vm_name_button_text = kParallelsShortName;
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM,
            vm_name_button_text)));
    vm_names.emplace_back(plugin_vm::kPluginVmName);
    vm_names_in_notification.emplace_back(vm_name);
    settings_sub_page =
        chromeos::settings::mojom::kPluginVmUsbPreferencesSubpagePath;
  }

  if (IsPlayStoreEnabledWithArcVmForProfile(profile())) {
    vm_name = l10n_util::GetStringUTF16(IDS_CROSUSB_NOTIFICATION_ARCVM);
    vm_name_button_text =
        l10n_util::GetStringUTF16(IDS_CROSUSB_NOTIFICATION_ARCVM_BUTTON);
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM,
            vm_name_button_text)));
    vm_names.emplace_back(arc::kArcVmName);
    vm_names_in_notification.emplace_back(vm_name);
    settings_sub_page =
        chromeos::settings::mojom::kArcVmUsbPreferencesSubpagePath;
  }

  if (bruschetta::IsInstalled(profile(), bruschetta::GetBruschettaAlphaId())) {
    vm_name = bruschetta::GetOverallVmName(profile());
    rich_notification_data.buttons.emplace_back(
        message_center::ButtonInfo(l10n_util::GetStringFUTF16(
            IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_VM, vm_name)));
    vm_names.emplace_back(bruschetta::kBruschettaVmName);
    vm_names_in_notification.emplace_back(vm_name);
    settings_sub_page =
        chromeos::settings::mojom::kBruschettaUsbPreferencesSubpagePath;
  }

  DCHECK(vm_names_in_notification.size());
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION, label,
      CombineVmNames(vm_names_in_notification));

  if (vm_names.size() > 1) {
    settings_sub_page = std::string();
  }

  std::string notification_id = CrosUsbDetector::MakeNotificationId(guid);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_TITLE),
      message, ui::ImageModel(), std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierUsb,
                                 NotificationCatalogName::kCrosUSBDetector),
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
  if (mount_error != MountError::kSuccess) {
    LOG(ERROR) << "Error unmounting USB drive: " << mount_error;
    success_ = false;
  }
}

}  // namespace

CrosUsbDeviceInfo::CrosUsbDeviceInfo(
    std::string guid,
    std::u16string label,
    std::optional<guest_os::GuestId> shared_guest_id,
    uint16_t vendor_id,
    uint16_t product_id,
    std::string serial_number,
    bool prompt_before_sharing)
    : guid(guid),
      label(label),
      shared_guest_id(shared_guest_id),
      vendor_id(vendor_id),
      product_id(product_id),
      serial_number(serial_number),
      prompt_before_sharing(prompt_before_sharing) {}
CrosUsbDeviceInfo::CrosUsbDeviceInfo(const CrosUsbDeviceInfo&) = default;
CrosUsbDeviceInfo::~CrosUsbDeviceInfo() = default;

std::string CrosUsbDetector::MakeNotificationId(const std::string& guid) {
  return "cros:" + guid;
}

CrosUsbDetector::DeviceClaim::DeviceClaim() = default;

CrosUsbDetector::DeviceClaim::~DeviceClaim() = default;

// static
CrosUsbDetector* CrosUsbDetector::Get() {
  return g_cros_usb_detector;
}

CrosUsbDetector::CrosUsbDetector() {
  DCHECK(!g_cros_usb_detector);
  g_cros_usb_detector = this;

  // If *ALL* interfaces of a device match the below list, no notification will
  // be shown.
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_CDC_DATA));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_HID));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PHYSICAL));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_AUDIO));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_STILL_IMAGE));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_MASS_STORAGE));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_VIDEO));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_BILLBOARD));
  guest_os_usb_int_all_filter_.emplace_back(
      UsbFilterByClassCode(USB_CLASS_PERSONAL_HEALTHCARE));

  // If *ANY* interfaces of a device match the below list, no notification will
  // be shown.
  guest_os_usb_int_any_filter_.emplace_back(UsbFilterByClassAndSubclassCode(
      USB_CLASS_COMM, USB_COMM_SUBCLASS_ETHERNET));

  CiceroneClient::Get()->AddObserver(this);
  ConciergeClient::Get()->AddVmObserver(this);
  VmPluginDispatcherClient::Get()->AddObserver(this);
  disks::DiskMountManager::GetInstance()->AddObserver(this);
}

CrosUsbDetector::~CrosUsbDetector() {
  DCHECK_EQ(this, g_cros_usb_detector);
  disks::DiskMountManager::GetInstance()->RemoveObserver(this);
  CiceroneClient::Get()->RemoveObserver(this);
  ConciergeClient::Get()->RemoveVmObserver(this);
  VmPluginDispatcherClient::Get()->RemoveObserver(this);
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
    std::string serial_number =
        device.info->serial_number.has_value()
            ? base::UTF16ToASCII(device.info->serial_number.value()).c_str()
            : "";
    result.emplace_back(
        device.info->guid, device.label, device.shared_guest_id,
        device.info->vendor_id, device.info->product_id, serial_number,
        /*prompt_before_sharing=*/
        device.shared_guest_id.has_value() || !device.mount_points.empty());
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
  PrefService* prefs = profile()->GetPrefs();
  if (!prefs->GetBoolean(ash::prefs::kUsbDetectorNotificationEnabled) ||
      !prefs->GetBoolean(guest_os::prefs::kGuestOsUSBNotificationEnabled)) {
    return false;
  }

  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile()) &&
      !plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile()) &&
      !IsPlayStoreEnabledWithArcVmForProfile(profile()) &&
      !bruschetta::IsInstalled(profile(), bruschetta::GetBruschettaAlphaId())) {
    return false;
  }

  bool all_filter_cleared =
      GetFilteredInterfacesMask(guest_os_usb_int_all_filter_, *device.info) !=
      0;
  bool any_filter_cleared =
      GetFilteredInterfacesMask(guest_os_usb_int_any_filter_, *device.info) ==
      GetUsbInterfaceBaseMask(*device.info);
  return all_filter_cleared && any_filter_cleared;
}

void CrosUsbDetector::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  const auto guest_id =
      guest_os::GuestId(signal.vm_name(), signal.container_name());
  for (auto& it : usb_devices_) {
    auto& device = it.second;
    if (device.shared_guest_id == guest_id && device.guest_port.has_value()) {
      VLOG(1) << "Connecting " << device.label << " to " << guest_id.vm_name
              << ":" << guest_id.container_name;
      AttachUsbDeviceToContainer(guest_id, *device.guest_port,
                                 device.info->guid, base::DoNothing());
    }
  }
}

void CrosUsbDetector::OnLxdContainerDeleted(
    const vm_tools::cicerone::LxdContainerDeletedSignal& signal) {
  if (signal.status() ==
      vm_tools::cicerone::LxdContainerDeletedSignal_Status_DELETED) {
    const auto guest_id =
        guest_os::GuestId(signal.vm_name(), signal.container_name());
    for (auto& it : usb_devices_) {
      auto& device = it.second;
      if (device.shared_guest_id == guest_id) {
        VLOG(1) << "Detaching " << device.label << " from deleted container "
                << guest_id.vm_name << ":" << guest_id.container_name;
        DetachUsbDeviceFromVm(guest_id.vm_name, device.info->guid,
                              base::DoNothing());
      }
    }
  }
}

void CrosUsbDetector::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  ConnectSharedDevicesOnVmStartup(signal.name());
}

void CrosUsbDetector::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  DisconnectSharedDevicesOnVmShutdown(signal.name());
}

void CrosUsbDetector::OnVmToolsStateChanged(
    const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal) {}

void CrosUsbDetector::OnVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  if (signal.vm_state() ==
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING) {
    ConnectSharedDevicesOnVmStartup(signal.vm_name());
  } else if (signal.vm_state() ==
             vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED) {
    DisconnectSharedDevicesOnVmShutdown(signal.vm_name());
  }
}

void CrosUsbDetector::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
    MountError error_code,
    const disks::DiskMountManager::MountPoint& mount_info) {
  if (mount_info.mount_type != MountType::kDevice ||
      error_code != MountError::kSuccess) {
    return;
  }

  const auto* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(
          mount_info.source_path);

  // This can be null if a drive is physically removed.
  if (!disk) {
    return;
  }

  for (auto& it : usb_devices_) {
    UsbDevice& device = it.second;
    if (disk->bus_number() ==
            base::checked_cast<int64_t>(device.info->bus_number) &&
        disk->device_number() ==
            base::checked_cast<int64_t>(device.info->port_number)) {
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

std::string UsbDeviceIdentifier(device::mojom::UsbDeviceInfoPtr& device_info) {
  std::string serial_number =
      device_info->serial_number.has_value()
          ? base::UTF16ToASCII(device_info->serial_number.value()).c_str()
          : "";
  return base::StringPrintf("%d:%d:%s", device_info->vendor_id,
                            device_info->product_id, serial_number.c_str());
}

void CrosUsbDetector::OnDeviceChecked(
    device::mojom::UsbDeviceInfoPtr device_info,
    bool hide_notification,
    bool allowed) {
  if (!allowed) {
    LOG(WARNING) << "Device not allowed by Permission Broker. vendor: 0x"
                 << std::hex << device_info->vendor_id << " product: 0x"
                 << device_info->product_id;
    return;
  }

  UsbDevice new_device;

  new_device.label = ProductLabelFromDevice(*device_info);

  // Storage devices already plugged in at log-in time will already be mounted.
  for (const auto& disk : disks::DiskMountManager::GetInstance()->disks()) {
    if (disk->bus_number() ==
            base::checked_cast<int64_t>(device_info->bus_number) &&
        disk->device_number() ==
            base::checked_cast<int64_t>(device_info->port_number) &&
        disk->is_mounted()) {
      new_device.mount_points.insert(disk->mount_path());
    }
  }

  // Copy fields prior to moving |device_info| and |new_device|.
  std::string guid = device_info->guid;
  std::u16string label = new_device.label;

  // If device exists in persistent passthrough dict, skip notifications and
  // connect it to the appropriate guest.
  PrefService* prefs = profile()->GetPrefs();
  const base::Value::Dict& persistent_passthrough_devices =
      prefs->GetDict(guest_os::prefs::kGuestOsUSBPersistentPassthroughDevices);

  const std::string* device = persistent_passthrough_devices.FindString(
      UsbDeviceIdentifier(device_info));

  new_device.info = std::move(device_info);
  auto result = usb_devices_.emplace(guid, std::move(new_device));

  if (!result.second) {
    LOG(ERROR) << "Ignoring USB device " << label << " as guid already exists.";
    return;
  }

  SignalUsbDeviceObservers();

  if (device) {
    const std::string& device_ref = CHECK_DEREF(device);
    std::optional<guest_os::GuestId> guest_id =
        guest_os::Deserialize(device_ref);
    if (guest_id.has_value()) {
      AttachUsbDeviceToGuest(guest_id.value(), guid, base::DoNothing());
      return;
    }
  }

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

  if (it->second.shared_guest_id.has_value()) {
    DetachUsbDeviceFromVm(it->second.shared_guest_id->vm_name, guid,
                          base::DoNothing());
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
    if (device.shared_guest_id.has_value() &&
        device.shared_guest_id->vm_name == vm_name) {
      VLOG(1) << "Connecting " << device.label << " to " << vm_name;
      // Clear any older guest_port setting.
      device.guest_port = std::nullopt;
      AttachUsbDeviceToGuest(*device.shared_guest_id, device.info->guid,
                             base::DoNothing());
    }
  }
}

void CrosUsbDetector::DisconnectSharedDevicesOnVmShutdown(
    const std::string& vm_name) {
  // Clear guest_port on shared devices when the VM shuts down.
  for (auto& it : usb_devices_) {
    auto& device = it.second;
    if (device.shared_guest_id.has_value() &&
        device.shared_guest_id->vm_name == vm_name) {
      VLOG(1) << device.label << " is disconnected from " << vm_name;
      device.guest_port = std::nullopt;
    }
  }
}

void CrosUsbDetector::AttachUsbDeviceToGuest(
    const guest_os::GuestId& guest_id,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  const auto& it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(WARNING) << "Attempted to attach device that does not exist: " << guid;
    std::move(callback).Run(false);
    return;
  }

  auto& device = it->second;

  // If we tried to share a device to a VM that wasn't started,
  // |shared_guest_id| would be set but |guest_port| would be empty. Once the VM
  // is started, we go through this flow again.
  if (device.guest_port.has_value()) {
    if (device.shared_guest_id == guest_id) {
      LOG(WARNING) << "Device " << device.label << " is already shared with vm "
                   << guest_id.vm_name;
      std::move(callback).Run(true);
      return;
    } else if (device.shared_guest_id->vm_name == guest_id.vm_name &&
               device.shared_guest_id->container_name !=
                   guest_id.container_name) {
      // The device is already shared with VM but in wrong container. In case
      // the new container is stopped, detach it from the old container first,
      // so that it can be attached later.
      DetachUsbDeviceFromContainer(
          guest_id.vm_name, *device.guest_port, device.info->guid,
          base::BindOnce(&CrosUsbDetector::ContainerAttachAfterDetach,
                         weak_ptr_factory_.GetWeakPtr(), guest_id,
                         *device.guest_port, guid, std::move(callback)));
      return;
    }
  }

  UnmountFilesystems(guest_id, guid, std::move(callback));
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
  if (!device.shared_guest_id.has_value() ||
      device.shared_guest_id->vm_name != vm_name) {
    LOG(WARNING) << "Failed to detach " << guid << " from " << vm_name
                 << ". It appears to be shared with "
                 << (device.shared_guest_id.has_value()
                         ? device.shared_guest_id->vm_name
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
    device.shared_guest_id = std::nullopt;
    SignalUsbDeviceObservers();
    std::move(callback).Run(/*success=*/true);
    return;
  }

  vm_tools::concierge::DetachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_guest_port(*device.guest_port);

  ConciergeClient::Get()->DetachUsbDevice(
      std::move(request),
      base::BindOnce(&CrosUsbDetector::OnUsbDeviceDetachFinished,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnListAttachedDevices(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (device::mojom::UsbDeviceInfoPtr& device_info : devices) {
    CrosUsbDetector::OnDeviceAdded(std::move(device_info),
                                   /*hide_notification*/ true);
  }
}

void CrosUsbDetector::UnmountFilesystems(
    const guest_os::GuestId& guest_id,
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
                     weak_ptr_factory_.GetWeakPtr(), guest_id, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnUnmountFilesystems(
    const guest_os::GuestId& guest_id,
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
  if (device.guest_port.has_value()) {
    DetachUsbDeviceFromVm(device.shared_guest_id->vm_name, guid,
                          base::BindOnce(&CrosUsbDetector::AttachAfterDetach,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         guest_id, guid, std::move(callback)));
  } else {
    // The device isn't attached.
    AttachAfterDetach(guest_id, guid, std::move(callback),
                      /*detach_success=*/true);
  }
}

void CrosUsbDetector::AttachAfterDetach(
    const guest_os::GuestId& guest_id,
    const std::string& guid,
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
  device.shared_guest_id = guest_id;

  auto claim_it = devices_claimed_.find(guid);
  if (claim_it != devices_claimed_.end()) {
    if (claim_it->second.device_file.is_valid()) {
      // We take a dup here which will be closed if DoVmAttach fails.
      base::ScopedFD device_fd(dup(claim_it->second.device_file.get()));
      DoVmAttach(guest_id, device.info.Clone(), std::move(device_fd),
                 std::move(callback));
    } else {
      LOG(WARNING) << "Device " << guid << " already claimed and awaiting fd.";
      std::move(callback).Run(false);
    }
    return;
  }

  VLOG(1) << "Opening " << guid;

  base::ScopedFD read_end, write_end;
  if (!base::CreatePipe(&read_end, &write_end, /*non_blocking=*/true)) {
    LOG(ERROR) << "Couldn't create pipe for " << guid;
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "Saving lifeline_fd " << write_end.get();
  devices_claimed_[guid].lifeline_file = std::move(write_end);

  // Open a file descriptor to pass to CrostiniManager & Concierge.
  device_manager_->OpenFileDescriptor(
      guid, kAllInterfacesMask, mojo::PlatformHandle(std::move(read_end)),
      base::BindOnce(&CrosUsbDetector::OnAttachUsbDeviceOpened,
                     weak_ptr_factory_.GetWeakPtr(), guest_id,
                     device.info.Clone(), std::move(callback)));

  // Close any associated notifications (the user isn't using them). This
  // destroys the CrosUsbNotificationDelegate and vm_name and guid args may be
  // invalid after Close.
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(guid));
}

void CrosUsbDetector::OnAttachUsbDeviceOpened(
    const guest_os::GuestId& guest_id,
    device::mojom::UsbDeviceInfoPtr device_info,
    base::OnceCallback<void(bool success)> callback,
    base::File file) {
  if (!file.IsValid()) {
    LOG(ERROR) << "Permission broker refused access to USB device";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  devices_claimed_[device_info->guid].device_file =
      base::ScopedFD(file.Duplicate().TakePlatformFile());
  if (!manager()) {
    LOG(ERROR) << "Attaching device without Crostini manager instance";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  DoVmAttach(guest_id, device_info.Clone(),
             base::ScopedFD(file.TakePlatformFile()), std::move(callback));
}

void CrosUsbDetector::DoVmAttach(
    const guest_os::GuestId& guest_id,
    device::mojom::UsbDeviceInfoPtr device_info,
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> callback) {
  vm_tools::concierge::AttachUsbDeviceRequest request;
  request.set_vm_name(guest_id.vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_bus_number(device_info->bus_number);
  request.set_port_number(device_info->port_number);
  request.set_vendor_id(device_info->vendor_id);
  request.set_product_id(device_info->product_id);

  ConciergeClient::Get()->AttachUsbDevice(
      std::move(fd), std::move(request),
      base::BindOnce(&CrosUsbDetector::OnUsbDeviceAttachFinished,
                     weak_ptr_factory_.GetWeakPtr(), guest_id,
                     std::move(device_info), std::move(callback)));
}

void CrosUsbDetector::OnUsbDeviceAttachFinished(
    const guest_os::GuestId& guest_id,
    device::mojom::UsbDeviceInfoPtr device_info,
    base::OnceCallback<void(bool success)> callback,
    std::optional<vm_tools::concierge::AttachUsbDeviceResponse> response) {
  bool success = true;
  if (!response) {
    LOG(ERROR) << "Failed to attach USB device, empty dbus response";
    success = false;
  } else if (!response->success()) {
    LOG(ERROR) << "Failed to attach USB device, " << response->reason();
    success = false;
  }

  if (success) {
    auto it = usb_devices_.find(device_info->guid);
    if (it == usb_devices_.end()) {
      LOG(WARNING) << "Dbus response indicates successful attach but device "
                   << "info was missing for " << device_info->guid;
      success = false;
    } else {
      it->second.shared_guest_id = guest_id;
      it->second.guest_port = response->guest_port();
    }
  }

  PrefService* prefs = profile()->GetPrefs();
  if (success &&
      prefs->GetBoolean(
          guest_os::prefs::kGuestOsUSBPersistentPassthroughEnabled)) {
    ScopedDictPrefUpdate update(
        prefs, guest_os::prefs::kGuestOsUSBPersistentPassthroughDevices);
    base::Value::Dict& devices = update.Get();
    std::string device_identifier = UsbDeviceIdentifier(device_info);
    // there are 3 possible scenarios here:
    // 1 - device was not in list. in this case we definitely want to add it.
    // 2 - device was in list for a different guest. in this case we want to
    //     override the previous state.
    // 3 - device was in list, with the current guest. we already have to
    //     serialize the guest_id to check, so not much more different in
    //     comparing vs writing the same thing back again.
    devices.Set(device_identifier, guest_id.Serialize());
  }

  if (success && !guest_id.container_name.empty()) {
    AttachUsbDeviceToContainer(guest_id, response->guest_port(),
                               device_info->guid, std::move(callback));
  } else {
    SignalUsbDeviceObservers();
    std::move(callback).Run(success);
  }
}

void CrosUsbDetector::AttachUsbDeviceToContainer(
    const guest_os::GuestId& guest_id,
    uint8_t guest_port,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  vm_tools::cicerone::AttachUsbToContainerRequest request;
  request.set_vm_name(guest_id.vm_name);
  request.set_container_name(guest_id.container_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_port_num(static_cast<int32_t>(guest_port));

  CiceroneClient::Get()->AttachUsbToContainer(
      std::move(request),
      base::BindOnce(&CrosUsbDetector::OnContainerAttachFinished,
                     weak_ptr_factory_.GetWeakPtr(), guest_id, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnContainerAttachFinished(
    const guest_os::GuestId& guest_id,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    std::optional<vm_tools::cicerone::AttachUsbToContainerResponse> response) {
  bool success = true;
  if (!response) {
    LOG(ERROR) << "Failed to attach USB device, empty dbus response";
    success = false;
  } else if (response->status() !=
             vm_tools::cicerone::AttachUsbToContainerResponse_Status_OK) {
    LOG(ERROR) << "Failed to attach USB device, " << response->failure_reason();
    success = false;
  }

  if (success) {
    const auto& it = usb_devices_.find(guid);
    if (it == usb_devices_.end()) {
      LOG(WARNING) << "Dbus response indicates successful attach but device "
                   << "info was missing for " << guid;
      success = false;
    } else {
      it->second.shared_guest_id = guest_id;
    }
  }

  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::DetachUsbDeviceFromContainer(
    const std::string& vm_name,
    uint8_t guest_port,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  vm_tools::cicerone::DetachUsbFromContainerRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_port_num(static_cast<int32_t>(guest_port));

  CiceroneClient::Get()->DetachUsbFromContainer(
      std::move(request),
      base::BindOnce(&CrosUsbDetector::OnContainerDetachFinished,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnContainerDetachFinished(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    std::optional<vm_tools::cicerone::DetachUsbFromContainerResponse>
        response) {
  bool success = true;
  if (!response) {
    LOG(ERROR) << "Failed to attach USB device, empty dbus response";
    success = false;
  } else if (response->status() !=
             vm_tools::cicerone::DetachUsbFromContainerResponse_Status_OK) {
    LOG(ERROR) << "Failed to attach USB device, " << response->failure_reason();
    success = false;
  }

  if (success) {
    const auto& it = usb_devices_.find(guid);
    if (it == usb_devices_.end()) {
      LOG(WARNING) << "Dbus response indicates successful detach but device "
                   << "info was missing for " << guid;
      success = false;
    } else {
      it->second.shared_guest_id->container_name = "";
    }
  }

  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::ContainerAttachAfterDetach(
    const guest_os::GuestId& guest_id,
    uint8_t guest_port,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    bool detach_success) {
  if (!detach_success) {
    LOG(ERROR) << "Failed to detach from container before attach";
    std::move(callback).Run(false);
    return;
  }

  const auto& it = usb_devices_.find(guid);
  if (it == usb_devices_.end()) {
    LOG(ERROR) << "No device info for " << guid;
    std::move(callback).Run(false);
    return;
  }

  auto& device = it->second;
  if (device.shared_guest_id->vm_name != guest_id.vm_name) {
    LOG(ERROR) << "Unexpected VM name for device " << guid;
    std::move(callback).Run(false);
    return;
  } else if (device.guest_port != guest_port) {
    LOG(ERROR) << "Unexpected guest port for device " << guid;
    std::move(callback).Run(false);
    return;
  }

  // Set the container name so if the container is stopped, the device will be
  // attached after the container starts.
  device.shared_guest_id->container_name = guest_id.container_name;
  AttachUsbDeviceToContainer(guest_id, guest_port, guid, std::move(callback));
}

void CrosUsbDetector::OnUsbDeviceDetachFinished(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    std::optional<vm_tools::concierge::DetachUsbDeviceResponse> response) {
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
    it->second.shared_guest_id = std::nullopt;
    it->second.guest_port = std::nullopt;
  }
  RelinquishDeviceClaim(guid);
  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::RelinquishDeviceClaim(const std::string& guid) {
  auto it = devices_claimed_.find(guid);
  if (it != devices_claimed_.end()) {
    VLOG(1) << "Closing lifeline_fd " << it->second.lifeline_file.get();
    devices_claimed_.erase(it);
  } else {
    LOG(ERROR) << "Relinquishing device with no prior claim: " << guid;
  }
}

}  // namespace ash
