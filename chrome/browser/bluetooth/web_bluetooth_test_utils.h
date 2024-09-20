// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_WEB_BLUETOOTH_TEST_UTILS_H_
#define CHROME_BROWSER_BLUETOOTH_WEB_BLUETOOTH_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter();

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  void SetIsPresent(bool is_present);

  void SimulateDeviceAdvertisementReceived(
      const std::string& device_address,
      const std::optional<std::string>& advertisement_name =
          std::nullopt) const;

  // device::BluetoothAdapter implementation:
  void AddObserver(device::BluetoothAdapter::Observer* observer) override;

  bool IsPresent() const override;

  bool IsPowered() const override;

  device::BluetoothAdapter::ConstDeviceList GetDevices() const override;

  device::BluetoothDevice* GetDevice(const std::string& address) override;

  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> filter,
      base::OnceCallback<void(/*is_error*/ bool,
                              device::UMABluetoothDiscoverySessionOutcome)>
          callback) override;

 protected:
  ~FakeBluetoothAdapter() override;

  bool is_present_ = true;
};

class FakeBluetoothGattCharacteristic
    : public testing::NiceMock<device::MockBluetoothGattCharacteristic> {
 public:
  FakeBluetoothGattCharacteristic(device::MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const device::BluetoothUUID& uuid,
                                  Properties properties,
                                  Permissions permissions);

  ~FakeBluetoothGattCharacteristic();

  // Move-only class
  FakeBluetoothGattCharacteristic(const FakeBluetoothGattCharacteristic&) =
      delete;
  FakeBluetoothGattCharacteristic operator=(
      const FakeBluetoothGattCharacteristic&) = delete;

  void ReadRemoteCharacteristic(ValueCallback callback) override;

  void StartNotifySession(NotifySessionCallback callback,
                          ErrorCallback error_callback) override;

  void StopNotifySession(device::BluetoothGattNotifySession::Id session,
                         base::OnceClosure callback) override;

  bool IsNotifying() const override;

  // Do not call the readValue callback until midway through the completion
  // of the startNotification callback registration.
  // https://crbug.com/1153426
  void DeferReadUntilNotificationStart();

  // Possibly trigger value characteristicvaluechanged events on the page
  // during the setup of startNotifications.
  // https://crbug.com/1153426.
  void EmitChangeNotificationAtNotificationStart();

 private:
  std::vector<uint8_t> value_;
  ValueCallback deferred_read_callback_;
  bool defer_read_until_notification_start_ = false;
  bool emit_value_change_at_notification_start_ = false;
  std::set<device::BluetoothGattNotifySession::Id> active_notify_sessions_;
};

class FakeBluetoothGattConnection
    : public testing::NiceMock<device::MockBluetoothGattConnection> {
 public:
  FakeBluetoothGattConnection(scoped_refptr<device::BluetoothAdapter> adapter,
                              const std::string& device_address);

  // Move-only class
  FakeBluetoothGattConnection(const FakeBluetoothGattConnection&) = delete;
  FakeBluetoothGattConnection operator=(const FakeBluetoothGattConnection&) =
      delete;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(device::MockBluetoothAdapter* adapter,
                      const std::string& address);

  void CreateGattConnection(
      device::BluetoothDevice::GattConnectionCallback callback,
      std::optional<device::BluetoothUUID> service_uuid =
          std::nullopt) override;

  bool IsGattServicesDiscoveryComplete() const override;

  device::BluetoothRemoteGattService* GetGattService(
      const std::string& identifier) const override;

  std::vector<device::BluetoothRemoteGattService*> GetGattServices()
      const override;

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  bool gatt_services_discovery_complete_ = false;
};

class FakeBluetoothChooser : public content::BluetoothChooser {
 public:
  FakeBluetoothChooser(content::BluetoothChooser::EventHandler event_handler,
                       const std::optional<std::string>& device_to_select);
  ~FakeBluetoothChooser() override;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

  // Move-only class
  FakeBluetoothChooser(const FakeBluetoothChooser&) = delete;
  FakeBluetoothChooser& operator=(const FakeBluetoothChooser&) = delete;

 private:
  content::BluetoothChooser::EventHandler event_handler_;
  std::optional<std::string> device_to_select_;
};

class TestBluetoothDelegate : public ChromeBluetoothDelegate {
 public:
  TestBluetoothDelegate();
  ~TestBluetoothDelegate() override;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  void UseRealChooser();

  void SetDeviceToSelect(const std::string& device_address);

 protected:
  // content::BluetoothDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;

  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;

 private:
  std::optional<std::string> device_to_select_;
  bool use_real_chooser_ = false;
};

class BluetoothTestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  BluetoothTestContentBrowserClient();
  ~BluetoothTestContentBrowserClient() override;
  BluetoothTestContentBrowserClient(const BluetoothTestContentBrowserClient&) =
      delete;
  BluetoothTestContentBrowserClient& operator=(
      const BluetoothTestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate();

 protected:
  // ChromeContentBrowserClient:
  content::BluetoothDelegate* GetBluetoothDelegate() override;

 private:
  TestBluetoothDelegate bluetooth_delegate_;
};

#endif  // CHROME_BROWSER_BLUETOOTH_WEB_BLUETOOTH_TEST_UTILS_H_
