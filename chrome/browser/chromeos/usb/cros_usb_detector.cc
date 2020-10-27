// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb/cros_usb_detector.h"

#include <fcntl.h>

#include <string>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
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

void RecordNotificationClosure(CrosUsbNotificationClosed disposition) {
  UMA_HISTOGRAM_ENUMERATION("CrosUsb.NotificationClosed", disposition);
}

base::string16 ProductLabelFromDevice(
    const device::mojom::UsbDeviceInfoPtr& device_info) {
  base::string16 product_label =
      l10n_util::GetStringUTF16(IDS_CROSUSB_UNKNOWN_DEVICE);
  if (device_info->product_name.has_value() &&
      !device_info->product_name->empty()) {
    product_label = device_info->product_name.value();
  } else if (device_info->manufacturer_name.has_value() &&
             !device_info->manufacturer_name->empty()) {
    product_label =
        l10n_util::GetStringFUTF16(IDS_CROSUSB_UNKNOWN_DEVICE_FROM_MANUFACTURER,
                                   device_info->manufacturer_name.value());
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
  explicit CrosUsbNotificationDelegate(
      const std::string& notification_id,
      device::mojom::UsbDeviceInfoPtr device_info,
      std::vector<std::string> vm_names,
      std::string settings_sub_page)
      : notification_id_(notification_id),
        device_info_(std::move(device_info)),
        vm_names_(std::move(vm_names)),
        settings_sub_page_(std::move(settings_sub_page)),
        disposition_(CrosUsbNotificationClosed::kUnknown) {}

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
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
    RecordNotificationClosure(disposition_);
  }

 private:
  ~CrosUsbNotificationDelegate() override = default;
  void HandleConnectToVm(const std::string& vm_name) {
    disposition_ = CrosUsbNotificationClosed::kConnectToLinux;
    chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
    if (detector) {
      detector->AttachUsbDeviceToVm(vm_name, device_info_->guid,
                                    base::DoNothing());
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
  device::mojom::UsbDeviceInfoPtr device_info_;
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

void ShowNotificationForDevice(device::mojom::UsbDeviceInfoPtr device_info) {
  message_center::RichNotificationData rich_notification_data;
  std::vector<std::string> vm_names;
  std::string settings_sub_page;
  base::string16 vm_name;
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

  base::string16 message;
  if (vm_names.size() == 1) {
    message = l10n_util::GetStringFUTF16(
        IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION,
        ProductLabelFromDevice(device_info), vm_name);
  } else {
    // Note: we assume right now that multi-VM is Linux and Plugin VM.
    message = l10n_util::GetStringFUTF16(
        IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_LINUX_PLUGIN_VM,
        ProductLabelFromDevice(device_info));
    settings_sub_page = std::string();
  }

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device_info->guid);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_TITLE),
      message, gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierUsb),
      rich_notification_data,
      base::MakeRefCounted<CrosUsbNotificationDelegate>(
          notification_id, std::move(device_info), std::move(vm_names),
          std::move(settings_sub_page)));
  SystemNotificationHelper::GetInstance()->Display(notification);
}

}  // namespace

CrosUsbDeviceInfo::CrosUsbDeviceInfo() = default;
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
}

CrosUsbDetector::~CrosUsbDetector() {
  DCHECK_EQ(this, g_cros_usb_detector);
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

const std::vector<CrosUsbDeviceInfo>& CrosUsbDetector::GetConnectedDevices()
    const {
  return usb_devices_;
}

std::vector<CrosUsbDeviceInfo> CrosUsbDetector::GetDevicesSharableWithCrostini()
    const {
  auto devices = usb_devices_;
  base::EraseIf(devices, [](const CrosUsbDeviceInfo& device) {
    return !device.sharable_with_crostini;
  });
  return devices;
}

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

bool CrosUsbDetector::ShouldShowNotification(
    const device::mojom::UsbDeviceInfo& device_info,
    uint32_t allowed_interfaces_mask) {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile()) &&
      !plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile())) {
    return false;
  }
  if (device::UsbDeviceFilterMatches(*adb_device_filter_, device_info) ||
      device::UsbDeviceFilterMatches(*fastboot_device_filter_, device_info)) {
    VLOG(1) << "Adb or fastboot device found";
    return true;
  }
  if ((GetFilteredInterfacesMask(guest_os_classes_without_notif_, device_info) &
       allowed_interfaces_mask) != 0) {
    VLOG(1) << "At least one notifiable interface found for device";
    // Only notify if no interfaces were suppressed.
    return GetUsbInterfaceBaseMask(device_info) == allowed_interfaces_mask;
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
  CrosUsbDeviceInfo new_device;
  new_device.guid = device_info->guid;
  new_device.label = ProductLabelFromDevice(device_info);

  const bool has_supported_interface =
      device::UsbDeviceFilterMatches(*adb_device_filter_, *device_info) ||
      device::UsbDeviceFilterMatches(*fastboot_device_filter_, *device_info);

  new_device.allowed_interfaces_mask =
      GetFilteredInterfacesMask(guest_os_classes_blocked_, *device_info);

  new_device.sharable_with_crostini =
      has_supported_interface || new_device.allowed_interfaces_mask != 0;

  usb_devices_.push_back(new_device);
  available_device_info_.emplace(device_info->guid, device_info.Clone());
  SignalUsbDeviceObservers();

  // Some devices should not trigger the notification.
  if (!new_device.sharable_with_crostini || hide_notification ||
      !ShouldShowNotification(*device_info,
                              new_device.allowed_interfaces_mask)) {
    VLOG(1) << "Not showing USB notification for " << new_device.label;
    return;
  }
  ShowNotificationForDevice(std::move(device_info));
}

void CrosUsbDetector::OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) {
  CrosUsbDetector::OnDeviceAdded(std::move(device), false);
}

void CrosUsbDetector::OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info,
                                    bool hide_notification) {
  device_manager_->CheckAccess(
      device_info->guid,
      base::BindOnce(&CrosUsbDetector::OnDeviceChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(device_info),
                     hide_notification));
}

void CrosUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(device_info->guid));

  std::string guid = device_info->guid;
  for (const auto& device : usb_devices_) {
    if (device.guid == guid && device.shared_vm_name) {
      DetachUsbDeviceFromVm(*device.shared_vm_name, guid, base::DoNothing());
    }
  }
  const auto& start = std::remove_if(
      usb_devices_.begin(), usb_devices_.end(),
      [guid](const CrosUsbDeviceInfo& device) { return device.guid == guid; });
  if (start != usb_devices_.end()) {
    usb_devices_.erase(start, usb_devices_.end());
  }
  available_device_info_.erase(guid);
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
  for (auto& device : usb_devices_) {
    if (device.shared_vm_name == vm_name) {
      VLOG(1) << "Connecting " << device.label << " to " << vm_name;
      // Clear any older guest_port setting.
      device.guest_port = base::nullopt;
      AttachUsbDeviceToVm(vm_name, device.guid, base::DoNothing());
    }
  }
}

bool CrosUsbDetector::IsDeviceAlreadySharedWithVm(const std::string& vm_name,
                                                  const std::string& guid) {
  for (const auto& device : usb_devices_) {
    if (device.guid == guid && device.shared_vm_name == vm_name &&
        device.guest_port) {
      VLOG(1) << "Device " << device.label << " is already shared with vm "
              << vm_name;
      return true;
    }
  }
  return false;
}

void CrosUsbDetector::AttachUsbDeviceToVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  if (IsDeviceAlreadySharedWithVm(vm_name, guid)) {
    std::move(callback).Run(true);
    return;
  }
  uint32_t allowed_interfaces_mask = 0;
  for (auto& device : usb_devices_) {
    if (device.guid == guid) {
      // Detach first if device is attached elsewhere
      if (device.shared_vm_name && device.shared_vm_name != vm_name) {
        DetachUsbDeviceFromVm(
            *device.shared_vm_name, guid,
            base::BindOnce(&CrosUsbDetector::AttachAfterDetach,
                           weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                           std::move(callback)));
        return;
      }

      // Mark the USB device shared so that we know to reattach it on VM
      // restart.
      // Setting this flag early also allows the UI not to flicker because of
      // the notification resulting from the default VM detach below.
      device.shared_vm_name = vm_name;
      allowed_interfaces_mask = device.allowed_interfaces_mask;
      // The guest port will be set on completion.
      break;
    }
  }
  auto it = available_device_info_.find(guid);
  if (it == available_device_info_.end()) {
    LOG(ERROR) << "No device info for " << guid;
    std::move(callback).Run(false);
    return;
  }

  const auto& device_info = it->second;

  auto claim_it = devices_claimed_.find(guid);
  if (claim_it != devices_claimed_.end()) {
    if (claim_it->second.device_file.IsValid()) {
      // We take a dup here which will be closed if DoVmAttach fails.
      base::ScopedFD device_fd(
          claim_it->second.device_file.Duplicate().TakePlatformFile());
      DoVmAttach(vm_name, device_info.Clone(), std::move(device_fd),
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
                     device_info.Clone(), std::move(callback)));

  // Close any associated notifications (the user isn't using them). This
  // destroys the CrosUsbNotificationDelegate and vm_name and guid args may be
  // invalid after Close.
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(guid));
}

void CrosUsbDetector::DetachUsbDeviceFromVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  const auto& it = available_device_info_.find(guid);
  if (it == available_device_info_.end()) {
    // If there wasn't an existing attachment, then removal is a no-op and
    // always succeeds
    LOG(ERROR) << "No device found to detach " << guid;
    std::move(callback).Run(/*success=*/true);
    return;
  }

  base::Optional<uint8_t> guest_port;
  for (auto& device : usb_devices_) {
    if (device.guid != guid)
      continue;

    guest_port = device.guest_port;
    if (device.shared_vm_name == vm_name && guest_port)
      break;

    LOG(WARNING) << "Failed to detach " << guid << " from " << vm_name
                 << ". It appears to be shared with "
                 << (device.shared_vm_name ? *device.shared_vm_name
                                           : "[not shared]")
                 << " at port "
                 << (guest_port ? base::NumberToString(*guest_port)
                                : "[not attached]")
                 << ".";
    if (device.shared_vm_name == vm_name) {
      // The VM hasn't been started yet, attaching is in progress, or attaching
      // failed.
      // TODO(timloh): Check what happens if attaching to a different VM races
      // with an in progress attach.
      device.shared_vm_name = base::nullopt;
      std::move(callback).Run(/*success=*/true);
      return;
    }
    std::move(callback).Run(/*success=*/false);
    return;
  }

  vm_tools::concierge::DetachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(profile()));
  request.set_guest_port(*guest_port);

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
    for (auto& device : usb_devices_) {
      if (device.guid == guid) {
        device.shared_vm_name = vm_name;
        device.guest_port = response->guest_port();
        break;
      }
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

  for (auto& device : usb_devices_) {
    if (device.guid == guid) {
      device.shared_vm_name = base::nullopt;
      device.guest_port = base::nullopt;
      break;
    }
  }
  RelinquishDeviceClaim(guid);
  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

void CrosUsbDetector::AttachAfterDetach(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to detatch before attach";
    std::move(callback).Run(false);
    return;
  }
  AttachUsbDeviceToVm(vm_name, guid, std::move(callback));
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
