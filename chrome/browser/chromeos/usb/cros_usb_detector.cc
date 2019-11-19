// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/usb/cros_usb_detector.h"

#include <string>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

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
      device::mojom::UsbDeviceInfoPtr device_info)
      : notification_id_(notification_id),
        device_info_(std::move(device_info)),
        disposition_(CrosUsbNotificationClosed::kUnknown) {}

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    disposition_ = CrosUsbNotificationClosed::kUnknown;
    if (button_index && button_index.value() == 0) {
      HandleConnectToVm();
    } else {
      HandleShowSettings();
    }
  }

  void Close(bool by_user) override {
    if (by_user)
      disposition_ = chromeos::CrosUsbNotificationClosed::kByUser;
    RecordNotificationClosure(disposition_);
  }

 private:
  ~CrosUsbNotificationDelegate() override = default;
  void HandleConnectToVm() {
    disposition_ = CrosUsbNotificationClosed::kConnectToLinux;
    chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
    if (detector) {
      detector->AttachUsbDeviceToVm(crostini::kCrostiniDefaultVmName,
                                    device_info_->guid, base::DoNothing());
      return;
    }
    Close(false);
  }

  void HandleShowSettings() {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile(), chrome::kCrostiniSharedUsbDevicesSubPage);
    Close(false);
  }

  std::string notification_id_;
  device::mojom::UsbDeviceInfoPtr device_info_;
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
  rich_notification_data.small_image = gfx::Image(
      gfx::CreateVectorIcon(vector_icons::kUsbIcon, 64, gfx::kGoogleBlue800));
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;
  rich_notification_data.buttons.emplace_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_CROSUSB_NOTIFICATION_BUTTON_CONNECT_TO_LINUX)));

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device_info->guid);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_CROSUSB_DEVICE_DETECTED_NOTIFICATION,
                                 ProductLabelFromDevice(device_info)),
      gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierUsb),
      rich_notification_data,
      base::MakeRefCounted<CrosUsbNotificationDelegate>(
          notification_id, std::move(device_info)));
  notification.SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(notification);
}

}  // namespace

CrosUsbDeviceInfo::CrosUsbDeviceInfo() = default;
CrosUsbDeviceInfo::CrosUsbDeviceInfo(const CrosUsbDeviceInfo&) = default;
CrosUsbDeviceInfo::~CrosUsbDeviceInfo() = default;

CrosUsbDeviceInfo::VmSharingInfo::VmSharingInfo() = default;
CrosUsbDeviceInfo::VmSharingInfo::VmSharingInfo(const VmSharingInfo&) = default;
CrosUsbDeviceInfo::VmSharingInfo::~VmSharingInfo() = default;

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
}

CrosUsbDetector::~CrosUsbDetector() {
  DCHECK_EQ(this, g_cros_usb_detector);
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
    content::GetSystemConnector()->Connect(
        device::mojom::kServiceName,
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
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile())) {
    return false;
  }
  if (device::UsbDeviceFilterMatches(*adb_device_filter_, device_info) ||
      device::UsbDeviceFilterMatches(*fastboot_device_filter_, device_info)) {
    return true;
  }
  return !device::UsbDeviceFilterMatchesAny(guest_os_classes_without_notif_,
                                            device_info);
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
  const bool has_blocked_interface = device::UsbDeviceFilterMatchesAny(
      guest_os_classes_blocked_, *device_info);
  new_device.sharable_with_crostini =
      has_supported_interface ||
      (!has_blocked_interface &&
       base::FeatureList::IsEnabled(features::kCrostiniUsbAllowUnsupported));

  usb_devices_.push_back(new_device);
  available_device_info_.emplace(device_info->guid, device_info.Clone());
  SignalUsbDeviceObservers();

  // Some devices should not trigger the notification.
  if (!new_device.sharable_with_crostini || hide_notification ||
      !ShouldShowNotification(*device_info)) {
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
    if (device.guid == guid) {
      for (const auto& sharing_info_pair : device.vm_sharing_info) {
        DetachUsbDeviceFromVm(sharing_info_pair.first, guid, base::DoNothing());
      }
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
    for (const auto& sharing_pair : device.vm_sharing_info) {
      if (sharing_pair.second.shared && sharing_pair.first == vm_name) {
        AttachUsbDeviceToVm(vm_name, device.guid, base::DoNothing());
      }
    }
  }
}

void CrosUsbDetector::AttachUsbDeviceToVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  for (auto& device : usb_devices_) {
    if (device.guid == guid) {
      // Mark the USB device shared so that we know to reattach it on VM
      // restart.
      // Setting this flag early also allows the UI not to flicker because of
      // the notification resulting from the default VM detach below.
      device.vm_sharing_info[vm_name].shared = true;
      // The guest port will be set on completion.
      break;
    }
  }
  auto it = available_device_info_.find(guid);
  if (it == available_device_info_.end()) {
    return;
  }

  const auto& device_info = it->second;
  // Close any associated notifications (the user isn't using them).
  SystemNotificationHelper::GetInstance()->Close(
      CrosUsbDetector::MakeNotificationId(guid));
  // Open a file descriptor to pass to CrostiniManager & Concierge.
  device_manager_->OpenFileDescriptor(
      guid, base::BindOnce(&CrosUsbDetector::OnAttachUsbDeviceOpened,
                           weak_ptr_factory_.GetWeakPtr(), vm_name,
                           device_info.Clone(), std::move(callback)));
}

void CrosUsbDetector::DetachUsbDeviceFromVm(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback) {
  const auto& it = available_device_info_.find(guid);
  if (it == available_device_info_.end()) {
    // If there wasn't an existing attachment, then removal is a no-op and
    // always succeeds
    std::move(callback).Run(/*success=*/true);
    return;
  }
  const auto& device_info = it->second;

  base::Optional<uint8_t> guest_port;
  for (const auto& device : usb_devices_) {
    if (device.guid == guid) {
      const auto it = device.vm_sharing_info.find(vm_name);
      if (it != device.vm_sharing_info.end()) {
        guest_port = it->second.guest_port;
        break;
      }
    }
  }

  if (!guest_port) {
    std::move(callback).Run(/*success=*/true);
    return;
  }
  manager()->DetachUsbDevice(
      vm_name, device_info.Clone(), *guest_port,
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
  base::ScopedFD fd(file.TakePlatformFile());
  if (!manager()) {
    LOG(ERROR) << "Attaching device without Crostini manager instance";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  for (const auto& device : usb_devices_) {
    if (device.guid == device_info->guid) {
      const auto it = device.vm_sharing_info.find(vm_name);
      if (it != device.vm_sharing_info.end() && it->second.guest_port) {
        // The device is already attached.
        std::move(callback).Run(/*success=*/true);
        return;
      }
    }
  }
  // TODO(b/123374026): Ideally CrostiniManager wouldn't be used for
  // attaching/detaching USB devices from non-Crostini VMs, e.g. ARCVM. It works
  // currently since CrostiniManager is mostly delegating to ConciergeClient but
  // it's a little confusing and fragile.
  const std::string guid = device_info->guid;
  manager()->AttachUsbDevice(
      vm_name, std::move(device_info), std::move(fd),
      base::BindOnce(&CrosUsbDetector::OnUsbDeviceAttachFinished,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guid,
                     std::move(callback)));
}

void CrosUsbDetector::OnUsbDeviceAttachFinished(
    const std::string& vm_name,
    const std::string& guid,
    base::OnceCallback<void(bool success)> callback,
    bool success,
    uint8_t guest_port) {
  if (success) {
    for (auto& device : usb_devices_) {
      if (device.guid == guid) {
        auto& vm_sharing_info = device.vm_sharing_info[vm_name];
        vm_sharing_info.shared = true;
        vm_sharing_info.guest_port = guest_port;
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
    bool success) {
  for (auto& device : usb_devices_) {
    if (device.guid == guid) {
      device.vm_sharing_info.erase(vm_name);
      break;
    }
  }
  SignalUsbDeviceObservers();
  std::move(callback).Run(success);
}

}  // namespace chromeos
