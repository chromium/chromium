// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_

#include <optional>
#include <string>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

class Device;

class RetroactivePairingDetectorImpl final
    : public RetroactivePairingDetector,
      public device::BluetoothAdapter::Observer,
      public PairerBroker::Observer,
      public MessageStreamLookup::Observer,
      public MessageStream::Observer,
      public SessionObserver {
 public:
  RetroactivePairingDetectorImpl(PairerBroker* pairer_broker,
                                 MessageStreamLookup* message_stream_lookup);
  RetroactivePairingDetectorImpl(const RetroactivePairingDetectorImpl&) =
      delete;
  RetroactivePairingDetectorImpl& operator=(
      const RetroactivePairingDetectorImpl&) = delete;
  ~RetroactivePairingDetectorImpl() override;

  // RetroactivePairingDetector:
  void AddObserver(RetroactivePairingDetector::Observer* observer) override;
  void RemoveObserver(RetroactivePairingDetector::Observer* observer) override;

 private:
  // There are three different ways we can retrieve the model id and ble
  // address from the MessageStream. The first: the MessageStream exists on
  // pair, so we retrieve it and check for model id and ble address. The
  // second: we need to wait for the MessageStream to connect and then parse
  // for ble address and model id. The third: we have the MessageStream but
  // no ble address or model id, so we add ourselves as an observer and wait
  // for the messages to arrive. This struct helps us store the data we have
  // while we wait for more information from the MessageStream per device in
  // |device_pairing_information_| map.
  struct RetroactivePairingInformation {
    std::string model_id;
    std::string ble_address;
    base::Time expiry_timestamp;
  };

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;

  // device::BluetoothAdapter::Observer
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // PairerBroker::Observer
  void OnDevicePaired(scoped_refptr<Device> device) override;
  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         std::optional<AccountKeyFailure> error) override;
  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override;

  // MessageStreamLookup::Observer
  void OnMessageStreamConnected(const std::string& device_address,
                                MessageStream* message_stream) override;

  // MessageStream::Observer
  void OnModelIdMessage(const std::string& device_address,
                        const std::string& model_id) override;
  void OnBleAddressUpdateMessage(const std::string& device_address,
                                 const std::string& ble_address) override;
  void OnDisconnected(const std::string& device_address) override;
  void OnMessageStreamDestroyed(const std::string& device_address) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Parses MessageStream messages for model id and BLE address, and
  // notifies observers if they exist within the |expiry_timeout| time period.
  void GetModelIdAndAddressFromMessageStream(const std::string& device_address,
                                             MessageStream* message_stream);

  // Checks |device_pairing_information_| for a BLE address and model id
  // needed for retroactive pairing, and notifies observers if within the
  // |expiry_timeout| time period.
  void CheckPairingInformation(const std::string& device_address);

  // Adds |device_pairing_information_| entry for a device at |device_address|
  // with the |expiry_timeout| field. BLE address and model id are added once
  // the `MessageStream` is connected.
  void AddDevicePairingInformation(const std::string& device_address);

  // Checks if the |device_pairing_information_| at |device_address| has
  // exceeded its expiry timeout. If so, removes all references to device in
  // |device_pairing_information_|, |potential_retroactive_addresses_|, and
  // removes an observer for a corresponding MessageStream and from
  // |message_streams_| if a MessageStream exists for the device, and returns
  // `true`. Otherwise if the device has not expired, returns `false`.
  bool CheckAndRemoveIfDeviceExpired(const std::string& device_address);

  // FastPairRepository::IsDeviceSavedToAccount callback
  void AttemptRetroactivePairing(const std::string& classic_address,
                                 bool is_device_saved_to_account);

  // FastPairRepository::CheckOptInStatus callback
  void OnCheckOptInStatus(const std::string& model_id,
                          const std::string& ble_address,
                          const std::string& classic_address,
                          nearby::fastpair::OptInStatus status);

  // Converts a Bluetooth device to a Fast Pair Device and notifies observers
  // that a device has been found to retroactively pair to.
  void NotifyDeviceFound(const std::string& model_id,
                         const std::string& ble_address,
                         const std::string& classic_address);
  void VerifyDeviceFound(const std::string& model_id,
                         const std::string& ble_address,
                         const std::string& classic_address);

  void RemoveDeviceInformation(const std::string& device_address);
  void RemoveDeviceInformationHelper(const std::string& device_address);

  // Iterates over |device_pairing_information_| and if a device's
  // |expiry_timestamp| has been reached, removes devices from
  // |device_pairing_information_|, |potential_retroactive_addresses_|, and
  // removes an observer for a corresponding MessageStream and from
  // |message_streams_| if a MessageStream exists for the device.
  void RemoveExpiredDevicesFromStoredDeviceData();

  // Gets or creates a Gatt connection to |device|.
  void CreateGattConnection(device::BluetoothDevice* device);

  // Internal method called when creating a FastPairGattServiceClient.
  void OnGattClientInitializedCallback(const std::string& address,
                                       std::optional<PairFailure> failure);

  // Internal method called to retrieve the model ID of a device.
  void OnReadModelId(
      const std::string& address,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // The classic pairing addresses of potential Retroactive Pair supported
  // devices that are found in the adapter. We have to store them and wait for a
  // MessageStream instance to be created for the device in order to fully
  // detect a Retroactive Pair device.
  base::flat_set<std::string> potential_retroactive_addresses_;

  // Map of the classic pairing address to their corresponding MessageStreams.
  base::flat_map<std::string, raw_ptr<MessageStream, CtnExperimental>>
      message_streams_;

  // Map of the classic pairing address to their corresponding model id and
  // ble address, if they exist.
  base::flat_map<std::string, RetroactivePairingInformation>
      device_pairing_information_;

  // Helps us keep track of whether the RetroactivePairingDetector has already
  // been instantiated when we get a logged-in event from the SessionObserver
  // so we can determine if we need to instantiate the objects.
  bool retroactive_pairing_detector_instatiated_ = false;

  raw_ptr<PairerBroker> pairer_broker_ = nullptr;
  raw_ptr<MessageStreamLookup> message_stream_lookup_ = nullptr;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::ObserverList<RetroactivePairingDetector::Observer> observers_;

  base::ScopedObservation<SessionController, SessionObserver>
      shell_observation_{this};
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::ScopedObservation<PairerBroker, PairerBroker::Observer>
      pairer_broker_observation_{this};
  base::ScopedObservation<MessageStreamLookup, MessageStreamLookup::Observer>
      message_stream_lookup_observation_{this};
  base::WeakPtrFactory<RetroactivePairingDetectorImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_RETROACTIVE_PAIRING_DETECTOR_IMPL_H_
