// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
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

// The BluetoothPairingNotificationDelegate handles user interaction with the
// pairing notification and sending the confirmation, rejection or cancellation
// back to the underlying device.
class BluetoothPairingNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  BluetoothPairingNotificationDelegate(scoped_refptr<BluetoothAdapter> adapter,
                                       const std::string& address,
                                       const std::string& notification_id);

  BluetoothPairingNotificationDelegate(
      const BluetoothPairingNotificationDelegate&) = delete;
  BluetoothPairingNotificationDelegate& operator=(
      const BluetoothPairingNotificationDelegate&) = delete;

 protected:
  ~BluetoothPairingNotificationDelegate() override;

  // message_center::NotificationDelegate overrides.
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

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
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
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

void ShowToast(const std::string& id,
               ToastCatalogName catalog_name,
               const std::u16string& text) {
  ash::ToastManager::Get()->Show(ash::ToastData(id, catalog_name, text));
}

}  // namespace

const char
    BluetoothNotificationController::kBluetoothDeviceDiscoverableToastId[] =
        "cros_bluetooth_device_discoverable_toast_id";

const char
    BluetoothNotificationController::kBluetoothDevicePairingNotificationId[] =
        "cros_bluetooth_device_pairing_notification_id";

BluetoothNotificationController::BluetoothNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  BluetoothAdapterFactory::Get()->GetAdapter(
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
  if (discoverable)
    NotifyAdapterDiscoverable();
}

void BluetoothNotificationController::DeviceAdded(BluetoothAdapter* adapter,
                                                  BluetoothDevice* device) {
  // Add the new device to the list of currently bonded devices; it doesn't
  // receive a notification since it's assumed it was previously notified.
  if (device->IsBonded()) {
    bonded_devices_.insert(device->GetAddress());
  }
}

void BluetoothNotificationController::DeviceChanged(BluetoothAdapter* adapter,
                                                    BluetoothDevice* device) {
  // If the device is already in the list of bonded devices, then don't
  // notify.
  if (base::Contains(bonded_devices_, device->GetAddress())) {
    return;
  }

  // Otherwise if it's marked as bonded then it must be newly bonded, so
  // notify the user about that.
  if (device->IsBonded()) {
    bonded_devices_.insert(device->GetAddress());
    NotifyBondedDevice(device);
  }
}

void BluetoothNotificationController::DeviceRemoved(BluetoothAdapter* adapter,
                                                    BluetoothDevice* device) {
  bonded_devices_.erase(device->GetAddress());
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
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_DISPLAY_PINCODE,
      device->GetNameForDisplay(), base::UTF8ToUTF16(pincode));

  NotifyPairing(device, message, false);
}

void BluetoothNotificationController::DisplayPasskey(BluetoothDevice* device,
                                                     uint32_t passkey) {
  std::u16string message = l10n_util::GetStringFUTF16(
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
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_CONFIRM_PASSKEY,
      device->GetNameForDisplay(),
      base::UTF8ToUTF16(base::StringPrintf("%06i", passkey)));

  NotifyPairing(device, message, true);
}

void BluetoothNotificationController::AuthorizePairing(
    BluetoothDevice* device) {
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_AUTHORIZE_PAIRING,
      device->GetNameForDisplay());

  NotifyPairing(device, message, true);
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

  // Build a list of the currently bonded devices; these don't receive
  // notifications since it's assumed they were previously notified.
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (BluetoothAdapter::DeviceList::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    const BluetoothDevice* device = *iter;
    if (device->IsBonded()) {
      bonded_devices_.insert(device->GetAddress());
    }
  }
}

void BluetoothNotificationController::NotifyAdapterDiscoverable() {
  // Do not show toast in kiosk app mode or if user is not logged in. This
  // prevents toast from being queued before the session starts.
  if (Shell::Get()->session_controller()->IsRunningInAppMode() ||
      !Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return;
  }

  // If Nearby Share has made the local device discoverable, do not
  // unnecessarily display this toast.
  // TODO(crbug.com/1155669): Generalize this logic to prevent leaking Nearby
  // implementation details.
  auto* nearby_share_delegate = Shell::Get()->nearby_share_delegate();
  if (nearby_share_delegate &&
      (nearby_share_delegate->IsEnableHighVisibilityRequestActive() ||
       nearby_share_delegate->IsHighVisibilityOn())) {
    return;
  }

  ShowToast(
      kBluetoothDeviceDiscoverableToastId,
      ToastCatalogName::kBluetoothAdapterDiscoverable,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERABLE,
                                 base::UTF8ToUTF16(adapter_->GetName())));
}

void BluetoothNotificationController::NotifyPairing(
    BluetoothDevice* device,
    const std::u16string& message,
    bool with_buttons) {
  message_center::RichNotificationData optional;
  if (with_buttons) {
    optional.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ACCEPT)));
    optional.buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_REJECT)));
  }

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kBluetoothDevicePairingNotificationId, std::u16string() /* title */,
      message, std::u16string() /* display source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierBluetooth,
          NotificationCatalogName::kBluetoothPairingRequest),
      optional,
      base::MakeRefCounted<BluetoothPairingNotificationDelegate>(
          adapter_, device->GetAddress(),
          kBluetoothDevicePairingNotificationId),
      kNotificationBluetoothIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void BluetoothNotificationController::NotifyBondedDevice(
    BluetoothDevice* device) {
  // Remove the currently presented pairing notification; since only one
  // pairing request is queued at a time, this is guaranteed to be the device
  // that just became bonded. The notification will be handled by
  // BluetoothDeviceStatusUiHandler.
  if (message_center_->FindVisibleNotificationById(
          kBluetoothDevicePairingNotificationId)) {
    message_center_->RemoveNotification(kBluetoothDevicePairingNotificationId,
                                        false /* by_user */);
  }
}

}  // namespace ash
