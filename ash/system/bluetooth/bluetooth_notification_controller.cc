// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace {

const char kNotifierBluetooth[] = "ash.bluetooth";
const char kPairedNotificationPrefix[] =
    "cros_bluetooth_device_paired_notification_id-";

// The BluetoothPairingNotificationDelegate handles user interaction with the
// pairing notification and sending the confirmation, rejection or cancellation
// back to the underlying device.
class BluetoothPairingNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  BluetoothPairingNotificationDelegate(scoped_refptr<BluetoothAdapter> adapter,
                                       const std::string& address,
                                       const std::string& notification_id);

 protected:
  ~BluetoothPairingNotificationDelegate() override;

  // message_center::NotificationDelegate overrides.
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

 private:
  // Buttons that appear in notifications.
  enum Button { BUTTON_ACCEPT, BUTTON_REJECT };

  // Reference to the underlying Bluetooth Adapter, holding onto this
  // reference ensures the adapter object doesn't go out of scope while we have
  // a pending request and user interaction.
  scoped_refptr<BluetoothAdapter> adapter_;

  // Address of the device being paired.
  const std::string address_;
  const std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingNotificationDelegate);
};

BluetoothPairingNotificationDelegate::BluetoothPairingNotificationDelegate(
    scoped_refptr<BluetoothAdapter> adapter,
    const std::string& address,
    const std::string& notification_id)
    : adapter_(adapter), address_(address), notification_id_(notification_id) {}

BluetoothPairingNotificationDelegate::~BluetoothPairingNotificationDelegate() =
    default;

void BluetoothPairingNotificationDelegate::Close(bool by_user) {
  VLOG(1) << "Pairing notification closed. by_user = " << by_user;
  // Ignore notification closes generated as a result of pairing completion.
  if (!by_user)
    return;

  // Cancel the pairing of the device, if the object still exists.
  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (device)
    device->CancelPairing();
}

void BluetoothPairingNotificationDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (!button_index)
    return;

  VLOG(1) << "Pairing notification, button click: " << *button_index;
  // If the device object still exists, send the appropriate response either
  // confirming or rejecting the pairing.
  BluetoothDevice* device = adapter_->GetDevice(address_);
  if (device) {
    switch (*button_index) {
      case BUTTON_ACCEPT:
        device->ConfirmPairing();
        break;
      case BUTTON_REJECT:
        device->RejectPairing();
        break;
    }
  }

  // In any case, remove this pairing notification.
  MessageCenter::Get()->RemoveNotification(notification_id_,
                                           false /* by_user */);
}

}  // namespace

const char BluetoothNotificationController::
    kBluetoothDeviceDiscoverableNotificationId[] =
        "cros_bluetooth_device_discoverable_notification_id";

const char
    BluetoothNotificationController::kBluetoothDevicePairingNotificationId[] =
        "cros_bluetooth_device_pairing_notification_id";

// This class handles opening the Bluetooth Settings UI when the user clicks
// on the Paired Notification.
class BluetoothNotificationController::BluetoothPairedNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  BluetoothPairedNotificationDelegate() = default;

 protected:
  ~BluetoothPairedNotificationDelegate() override = default;

  // message_center::NotificationDelegate:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (TrayPopupUtils::CanOpenWebUISettings())
      Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothPairedNotificationDelegate);
};

BluetoothNotificationController::BluetoothNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  BluetoothAdapterFactory::GetAdapter(
      base::BindOnce(&BluetoothNotificationController::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothNotificationController::~BluetoothNotificationController() {
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_->RemovePairingDelegate(this);
    adapter_.reset();
  }
}

void BluetoothNotificationController::AdapterDiscoverableChanged(
    BluetoothAdapter* adapter,
    bool discoverable) {
  if (discoverable) {
    NotifyAdapterDiscoverable();
  } else {
    // Clear any previous discoverable notification.
    message_center_->RemoveNotification(
        kBluetoothDeviceDiscoverableNotificationId, false /* by_user */);
  }
}

void BluetoothNotificationController::DeviceAdded(BluetoothAdapter* adapter,
                                                  BluetoothDevice* device) {
  // Add the new device to the list of currently paired devices; it doesn't
  // receive a notification since it's assumed it was previously notified.
  if (device->IsPaired())
    paired_devices_.insert(device->GetAddress());
}

void BluetoothNotificationController::DeviceChanged(BluetoothAdapter* adapter,
                                                    BluetoothDevice* device) {
  // If the device is already in the list of paired devices, then don't
  // notify.
  if (paired_devices_.find(device->GetAddress()) != paired_devices_.end())
    return;

  // Otherwise if it's marked as paired then it must be newly paired, so
  // notify the user about that.
  if (device->IsPaired()) {
    paired_devices_.insert(device->GetAddress());
    NotifyPairedDevice(device);
  }
}

void BluetoothNotificationController::DeviceRemoved(BluetoothAdapter* adapter,
                                                    BluetoothDevice* device) {
  paired_devices_.erase(device->GetAddress());
}

void BluetoothNotificationController::RequestPinCode(BluetoothDevice* device) {
  // Cannot provide keyboard entry in a notification; these devices (old car
  // audio systems for the most part) will need pairing to be initiated from
  // the Chromebook.
  device->CancelPairing();
}

void BluetoothNotificationController::RequestPasskey(BluetoothDevice* device) {
  // Cannot provide keyboard entry in a notification; fortunately the spec
  // doesn't allow for this to be an option when we're receiving the pairing
  // request anyway.
  device->CancelPairing();
}

void BluetoothNotificationController::DisplayPinCode(
    BluetoothDevice* device,
    const std::string& pincode) {
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_DISPLAY_PINCODE,
      device->GetNameForDisplay(), base::UTF8ToUTF16(pincode));

  NotifyPairing(device, message, false);
}

void BluetoothNotificationController::DisplayPasskey(BluetoothDevice* device,
                                                     uint32_t passkey) {
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_DISPLAY_PASSKEY,
      device->GetNameForDisplay(),
      base::UTF8ToUTF16(base::StringPrintf("%06i", passkey)));

  NotifyPairing(device, message, false);
}

void BluetoothNotificationController::KeysEntered(BluetoothDevice* device,
                                                  uint32_t entered) {
  // Ignored since we don't have CSS in the notification to update.
}

void BluetoothNotificationController::ConfirmPasskey(BluetoothDevice* device,
                                                     uint32_t passkey) {
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_CONFIRM_PASSKEY,
      device->GetNameForDisplay(),
      base::UTF8ToUTF16(base::StringPrintf("%06i", passkey)));

  NotifyPairing(device, message, true);
}

void BluetoothNotificationController::AuthorizePairing(
    BluetoothDevice* device) {
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_AUTHORIZE_PAIRING,
      device->GetNameForDisplay());

  NotifyPairing(device, message, true);
}

// static
std::string BluetoothNotificationController::GetPairedNotificationId(
    const BluetoothDevice* device) {
  return kPairedNotificationPrefix + base::ToLowerASCII(device->GetAddress());
}

void BluetoothNotificationController::OnGetAdapter(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_.get());
  adapter_ = adapter;
  adapter_->AddObserver(this);
  adapter_->AddPairingDelegate(this,
                               BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  // Notify a user if the adapter is already in the discoverable state.
  if (adapter_->IsDiscoverable())
    NotifyAdapterDiscoverable();

  // Build a list of the currently paired devices; these don't receive
  // notifications since it's assumed they were previously notified.
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (BluetoothAdapter::DeviceList::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    const BluetoothDevice* device = *iter;
    if (device->IsPaired())
      paired_devices_.insert(device->GetAddress());
  }
}

void BluetoothNotificationController::NotifyAdapterDiscoverable() {
  message_center::RichNotificationData optional;

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kBluetoothDeviceDiscoverableNotificationId, base::string16() /* title */,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERABLE,
                                 base::UTF8ToUTF16(adapter_->GetName()),
                                 base::UTF8ToUTF16(adapter_->GetAddress())),
      base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierBluetooth),
      optional, nullptr, kNotificationBluetoothIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void BluetoothNotificationController::NotifyPairing(
    BluetoothDevice* device,
    const base::string16& message,
    bool with_buttons) {
  message_center::RichNotificationData optional;
  if (with_buttons) {
    optional.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ACCEPT)));
    optional.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_REJECT)));
  }

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kBluetoothDevicePairingNotificationId, base::string16() /* title */,
      message, base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierBluetooth),
      optional,
      base::MakeRefCounted<BluetoothPairingNotificationDelegate>(
          adapter_, device->GetAddress(),
          kBluetoothDevicePairingNotificationId),
      kNotificationBluetoothIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void BluetoothNotificationController::NotifyPairedDevice(
    BluetoothDevice* device) {
  // Remove the currently presented pairing notification; since only one
  // pairing request is queued at a time, this is guaranteed to be the device
  // that just became paired.
  if (message_center_->FindVisibleNotificationById(
          kBluetoothDevicePairingNotificationId)) {
    message_center_->RemoveNotification(kBluetoothDevicePairingNotificationId,
                                        false /* by_user */);
  }

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetPairedNotificationId(device),
      base::string16() /* title */,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED,
                                 device->GetNameForDisplay()),
      base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierBluetooth),
      message_center::RichNotificationData(),
      base::MakeRefCounted<BluetoothPairedNotificationDelegate>(),
      kNotificationBluetoothIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
