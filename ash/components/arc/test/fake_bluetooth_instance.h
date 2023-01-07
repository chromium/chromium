// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {
class BluetoothUUID;
}

namespace arc {

class FakeBluetoothInstance : public mojom::BluetoothInstance {
 public:
  class GattDBResult {
   public:
    GattDBResult(mojom::BluetoothAddressPtr&& remote_addr,
                 std::vector<mojom::BluetoothGattDBElementPtr>&& db);

    GattDBResult(const GattDBResult&) = delete;
    GattDBResult& operator=(const GattDBResult&) = delete;

    ~GattDBResult();

    const mojom::BluetoothAddressPtr& remote_addr() const {
      return remote_addr_;
    }

    const std::vector<mojom::BluetoothGattDBElementPtr>& db() const {
      return db_;
    }

   private:
    mojom::BluetoothAddressPtr remote_addr_;
    std::vector<mojom::BluetoothGattDBElementPtr> db_;
  };

  class LEDeviceFoundData {
   public:
    LEDeviceFoundData(mojom::BluetoothAddressPtr addr,
                      int32_t rssi,
                      const std::vector<uint8_t>& eir);

    LEDeviceFoundData(const LEDeviceFoundData&) = delete;
    LEDeviceFoundData& operator=(const LEDeviceFoundData&) = delete;

    ~LEDeviceFoundData();

    const mojom::BluetoothAddressPtr& addr() const { return addr_; }

    int32_t rssi() const { return rssi_; }

    const std::vector<uint8_t>& eir() const { return eir_; }

   private:
    mojom::BluetoothAddressPtr addr_;
    int32_t rssi_;
    std::vector<uint8_t> eir_;
  };

  class ConnectionStateChangedData {
   public:
    ConnectionStateChangedData(mojom::BluetoothAddressPtr addr,
                               device::BluetoothTransport device_type,
                               bool connected);

    ConnectionStateChangedData(const ConnectionStateChangedData&) = delete;
    ConnectionStateChangedData& operator=(const ConnectionStateChangedData&) =
        delete;

    ~ConnectionStateChangedData();

    const mojom::BluetoothAddressPtr& addr() const { return addr_; }
    device::BluetoothTransport device_type() const { return device_type_; }
    bool connected() const { return connected_; }

   private:
    mojom::BluetoothAddressPtr addr_;
    device::BluetoothTransport device_type_;
    bool connected_;
  };

  class LEConnectionStateChangeData {
   public:
    LEConnectionStateChangeData(mojom::BluetoothAddressPtr addr,
                                bool connected);

    LEConnectionStateChangeData(const LEConnectionStateChangeData&) = delete;
    LEConnectionStateChangeData& operator=(const LEConnectionStateChangeData&) =
        delete;

    ~LEConnectionStateChangeData();

    const mojom::BluetoothAddressPtr& addr() const { return addr_; }
    bool connected() const { return connected_; }

   private:
    mojom::BluetoothAddressPtr addr_;
    bool connected_;
  };

  FakeBluetoothInstance();

  FakeBluetoothInstance(const FakeBluetoothInstance&) = delete;
  FakeBluetoothInstance& operator=(const FakeBluetoothInstance&) = delete;

  ~FakeBluetoothInstance() override;

  // mojom::BluetoothInstance overrides:
  void Init(mojo::PendingRemote<mojom::BluetoothHost> host_remote,
            InitCallback callback) override;
  void OnAdapterProperties(
      mojom::BluetoothStatus status,
      std::vector<mojom::BluetoothPropertyPtr> properties) override;
  void OnDeviceFound(
      std::vector<mojom::BluetoothPropertyPtr> properties) override;
  void OnDevicePropertiesChanged(
      mojom::BluetoothAddressPtr remote_addr,
      std::vector<mojom::BluetoothPropertyPtr> properties) override;
  void OnDiscoveryStateChanged(mojom::BluetoothDiscoveryState state) override;
  void OnBondStateChanged(mojom::BluetoothStatus status,
                          mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothBondState state) override;
  void OnConnectionStateChanged(mojom::BluetoothAddressPtr remote_addr,
                                device::BluetoothTransport device_type,
                                bool connected) override;
  void OnLEDeviceFound(mojom::BluetoothAddressPtr addr,
                       int32_t rssi,
                       const std::vector<uint8_t>& eir) override;
  void OnLEConnectionStateChange(mojom::BluetoothAddressPtr remote_addr,
                                 bool connected) override;
  void OnLEDeviceAddressChange(mojom::BluetoothAddressPtr old_addr,
                               mojom::BluetoothAddressPtr new_addr) override;
  void OnSearchComplete(mojom::BluetoothAddressPtr remote_addr,
                        mojom::BluetoothGattStatus status) override;
  void OnGetGattDB(mojom::BluetoothAddressPtr remote_addr,
                   std::vector<mojom::BluetoothGattDBElementPtr> db) override;

  void OnGattNotify(mojom::BluetoothAddressPtr remote_addr,
                    mojom::BluetoothGattServiceIDPtr service_id,
                    mojom::BluetoothGattIDPtr char_id,
                    bool is_notify,
                    const std::vector<uint8_t>& value) override;

  void RequestGattRead(mojom::BluetoothAddressPtr address,
                       int32_t attribute_handle,
                       int32_t offset,
                       bool is_long,
                       mojom::BluetoothGattDBAttributeType attribute_type,
                       RequestGattReadCallback callback) override;

  void RequestGattWrite(mojom::BluetoothAddressPtr address,
                        int32_t attribute_handle,
                        int32_t offset,
                        const std::vector<uint8_t>& value,
                        mojom::BluetoothGattDBAttributeType attribute_type,
                        bool is_prepare,
                        RequestGattWriteCallback callback) override;

  void RequestGattExecuteWrite(
      mojom::BluetoothAddressPtr address,
      bool execute,
      RequestGattExecuteWriteCallback callback) override;

  void OnGetSdpRecords(
      mojom::BluetoothStatus status,
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      std::vector<mojom::BluetoothSdpRecordPtr> records) override;

  void OnMTUReceived(mojom::BluetoothAddressPtr remote_addr,
                     uint16_t mtu) override;

  void OnServiceChanged(mojom::BluetoothAddressPtr remote_addr) override;

  const std::vector<std::vector<mojom::BluetoothPropertyPtr>>&
  device_found_data() const {
    return device_found_data_;
  }

  const std::vector<std::vector<mojom::BluetoothPropertyPtr>>&
  device_properties_changed_data() const {
    return device_properties_changed_data_;
  }

  const std::vector<std::unique_ptr<LEDeviceFoundData>>& le_device_found_data()
      const {
    return le_device_found_data_;
  }

  const std::vector<std::unique_ptr<ConnectionStateChangedData>>&
  connection_state_changed_data() const {
    return connection_state_changed_data_;
  }

  const std::vector<std::unique_ptr<LEConnectionStateChangeData>>&
  le_connection_state_change_data() {
    return le_connection_state_change_data_;
  }

  const std::vector<std::unique_ptr<GattDBResult>>& gatt_db_result() const {
    return gatt_db_result_;
  }

  bool get_service_changed_flag() {
    return service_changed_flag_;
  }

  void reset_service_changed_flag() {
    service_changed_flag_ = false;
  }

 private:
  std::vector<std::vector<mojom::BluetoothPropertyPtr>> device_found_data_;
  std::vector<std::vector<mojom::BluetoothPropertyPtr>>
      device_properties_changed_data_;
  std::vector<std::unique_ptr<LEDeviceFoundData>> le_device_found_data_;
  std::vector<std::unique_ptr<ConnectionStateChangedData>>
      connection_state_changed_data_;
  std::vector<std::unique_ptr<LEConnectionStateChangeData>>
      le_connection_state_change_data_;
  std::vector<std::unique_ptr<GattDBResult>> gatt_db_result_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojo::Remote<mojom::BluetoothHost> host_remote_;

  // To indicate whether OnServiceChanged is called.
  bool service_changed_flag_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
