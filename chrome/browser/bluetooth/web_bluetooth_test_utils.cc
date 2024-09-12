// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/web_bluetooth_test_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"
#include "chrome/browser/bluetooth/web_bluetooth_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothGattNotifySession;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using ::device::MockBluetoothGattCharacteristic;
using ::device::MockBluetoothGattNotifySession;
using ::device::MockBluetoothGattService;

}  // namespace

FakeBluetoothAdapter::FakeBluetoothAdapter() = default;

void FakeBluetoothAdapter::SetIsPresent(bool is_present) {
  is_present_ = is_present;
}

void FakeBluetoothAdapter::SimulateDeviceAdvertisementReceived(
    const std::string& device_address,
    const std::optional<std::string>& advertisement_name) const {
  for (auto& observer : observers_) {
    observer.DeviceAdvertisementReceived(
        device_address, /*device_name=*/std::nullopt, advertisement_name,
        /*rssi=*/std::nullopt, /*tx_power=*/std::nullopt,
        /*appearance=*/std::nullopt,
        /*advertised_uuids=*/{}, /*service_data_map=*/{},
        /*manufacturer_data_map=*/{});
  }
}

void FakeBluetoothAdapter::AddObserver(
    device::BluetoothAdapter::Observer* observer) {
  device::BluetoothAdapter::AddObserver(observer);
}

bool FakeBluetoothAdapter::IsPresent() const {
  return is_present_;
}

bool FakeBluetoothAdapter::IsPowered() const {
  return true;
}

device::BluetoothAdapter::ConstDeviceList FakeBluetoothAdapter::GetDevices()
    const {
  device::BluetoothAdapter::ConstDeviceList devices;
  for (const auto& it : mock_devices_) {
    devices.push_back(it.get());
  }
  return devices;
}

device::BluetoothDevice* FakeBluetoothAdapter::GetDevice(
    const std::string& address) {
  for (const auto& it : mock_devices_) {
    if (it->GetAddress() == address) {
      return it.get();
    }
  }
  return nullptr;
}

void FakeBluetoothAdapter::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> filter,
    base::OnceCallback<void(/*is_error*/ bool,
                            device::UMABluetoothDiscoverySessionOutcome)>
        callback) {
  std::move(callback).Run(
      /*is_error=*/false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

FakeBluetoothAdapter::~FakeBluetoothAdapter() = default;

FakeBluetoothGattCharacteristic::FakeBluetoothGattCharacteristic(
    MockBluetoothGattService* service,
    const std::string& identifier,
    const BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions)
    : testing::NiceMock<MockBluetoothGattCharacteristic>(service,
                                                         identifier,
                                                         uuid,
                                                         properties,
                                                         permissions),
      value_({1}) {}

FakeBluetoothGattCharacteristic::~FakeBluetoothGattCharacteristic() = default;

void FakeBluetoothGattCharacteristic::ReadRemoteCharacteristic(
    ValueCallback callback) {
  if (!(GetProperties() & BluetoothGattCharacteristic::PROPERTY_READ)) {
    std::move(callback).Run(BluetoothGattService::GattErrorCode::kNotPermitted,
                            std::vector<uint8_t>());
    return;
  }
  if (defer_read_until_notification_start_) {
    DCHECK(!deferred_read_callback_);
    deferred_read_callback_ = std::move(callback);
    return;
  }
  std::move(callback).Run(/*error_code=*/std::nullopt, value_);
}

void FakeBluetoothGattCharacteristic::StartNotifySession(
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  if (!(GetProperties() & BluetoothGattCharacteristic::PROPERTY_NOTIFY)) {
    std::move(error_callback)
        .Run(BluetoothGattService::GattErrorCode::kNotPermitted);
    return;
  }
  auto fake_notify_session =
      std::make_unique<testing::NiceMock<MockBluetoothGattNotifySession>>(
          GetWeakPtr());
  active_notify_sessions_.insert(fake_notify_session->unique_id());

  if (deferred_read_callback_) {
    // A new value as a result of calling readValue().
    std::move(deferred_read_callback_).Run(/*error_code=*/std::nullopt, value_);
  }

  if (emit_value_change_at_notification_start_) {
    BluetoothAdapter* adapter = GetService()->GetDevice()->GetAdapter();
    adapter->NotifyGattCharacteristicValueChanged(this, value_);

    // NotifyGattCharacteristicValueChanged(...) posts a task to notify the
    // renderer of the change. Do the same for |callback| to ensure
    // StartNotifySession completes after the value change notification is
    // received.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(fake_notify_session)));
  } else {
    // Complete StartNotifySession normally.
    std::move(callback).Run(std::move(fake_notify_session));
  }
  EXPECT_TRUE(IsNotifying());
}

void FakeBluetoothGattCharacteristic::StopNotifySession(
    BluetoothGattNotifySession::Id session,
    base::OnceClosure callback) {
  EXPECT_TRUE(base::Contains(active_notify_sessions_, session));
  std::move(callback).Run();
}

bool FakeBluetoothGattCharacteristic::IsNotifying() const {
  return !active_notify_sessions_.empty();
}

// Do not call the readValue callback until midway through the completion
// of the startNotification callback registration.
// https://crbug.com/1153426
void FakeBluetoothGattCharacteristic::DeferReadUntilNotificationStart() {
  defer_read_until_notification_start_ = true;
}

// Possibly trigger value characteristicvaluechanged events on the page
// during the setup of startNotifications.
// https://crbug.com/1153426.
void FakeBluetoothGattCharacteristic::
    EmitChangeNotificationAtNotificationStart() {
  emit_value_change_at_notification_start_ = true;
}

FakeBluetoothGattConnection::FakeBluetoothGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : testing::NiceMock<device::MockBluetoothGattConnection>(adapter,
                                                             device_address) {}

FakeBluetoothDevice::FakeBluetoothDevice(device::MockBluetoothAdapter* adapter,
                                         const std::string& address)
    : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                     /*bluetooth_class=*/0u,
                                                     /*name=*/"Test Device",
                                                     address,
                                                     /*paired=*/true,
                                                     /*connected=*/true) {}

void FakeBluetoothDevice::CreateGattConnection(
    device::BluetoothDevice::GattConnectionCallback callback,
    std::optional<device::BluetoothUUID> service_uuid) {
  SetConnected(true);
  gatt_services_discovery_complete_ = true;
  std::move(callback).Run(
      std::make_unique<FakeBluetoothGattConnection>(adapter_, GetAddress()),
      /*error_code=*/std::nullopt);
}

bool FakeBluetoothDevice::IsGattServicesDiscoveryComplete() const {
  return gatt_services_discovery_complete_;
}

BluetoothRemoteGattService* FakeBluetoothDevice::GetGattService(
    const std::string& identifier) const {
  return GetMockService(identifier);
}

std::vector<device::BluetoothRemoteGattService*>
FakeBluetoothDevice::GetGattServices() const {
  return GetMockServices();
}

FakeBluetoothChooser::FakeBluetoothChooser(
    content::BluetoothChooser::EventHandler event_handler,
    const std::optional<std::string>& device_to_select)
    : event_handler_(event_handler), device_to_select_(device_to_select) {}

FakeBluetoothChooser::~FakeBluetoothChooser() = default;

// content::BluetoothChooser implementation:
void FakeBluetoothChooser::AddOrUpdateDevice(const std::string& device_id,
                                             bool should_update_name,
                                             const std::u16string& device_name,
                                             bool is_gatt_connected,
                                             bool is_paired,
                                             int signal_strength_level) {
  // Select the first device that is added if |device_to_select_| is not
  // populated.
  if (!device_to_select_) {
    event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
    return;
  }

  // Otherwise, select the added device if its device ID matches
  // |device_to_select_|.
  if (device_to_select_.value() == device_id) {
    event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
  }
}

TestBluetoothDelegate::TestBluetoothDelegate()
    : ChromeBluetoothDelegate(
          std::make_unique<ChromeBluetoothDelegateImplClient>()) {}

TestBluetoothDelegate::~TestBluetoothDelegate() = default;

void TestBluetoothDelegate::UseRealChooser() {
  EXPECT_FALSE(device_to_select_.has_value());
  use_real_chooser_ = true;
}

void TestBluetoothDelegate::SetDeviceToSelect(
    const std::string& device_address) {
  EXPECT_FALSE(use_real_chooser_);
  device_to_select_ = device_address;
}

std::unique_ptr<content::BluetoothChooser>
TestBluetoothDelegate::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  if (use_real_chooser_) {
    return ChromeBluetoothDelegate::RunBluetoothChooser(frame, event_handler);
  }
  return std::make_unique<FakeBluetoothChooser>(event_handler,
                                                device_to_select_);
}

std::unique_ptr<content::BluetoothScanningPrompt>
TestBluetoothDelegate::ShowBluetoothScanningPrompt(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler) {
  // Simulate that a prompt was accepted; no actual prompt is needed here.
  event_handler.Run(content::BluetoothScanningPrompt::Event::kAllow);
  return nullptr;
}

BluetoothTestContentBrowserClient::BluetoothTestContentBrowserClient() =
    default;

BluetoothTestContentBrowserClient::~BluetoothTestContentBrowserClient() =
    default;

TestBluetoothDelegate* BluetoothTestContentBrowserClient::bluetooth_delegate() {
  return &bluetooth_delegate_;
}

content::BluetoothDelegate*
BluetoothTestContentBrowserClient::GetBluetoothDelegate() {
  return &bluetooth_delegate_;
}
