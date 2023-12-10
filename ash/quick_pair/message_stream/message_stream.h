// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_

#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace ash {
namespace quick_pair {

// Receives MessageStreamMessage bytes from a given BluetoothSocket and uses
// the FastPairDataParser to parse a MessageStreamMessage from the data, and
// then notifies observers of the corresponding message data, and stores them.
class MessageStream {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Model ID message:
    // https://developers.google.com/nearby/fast-pair/spec#model_id_2
    virtual void OnModelIdMessage(const std::string& device_address,
                                  const std::string& model_id) {}

    // BLE Address Update message:
    // https://developers.google.com/nearby/fast-pair/spec#ble_address_2
    virtual void OnBleAddressUpdateMessage(const std::string& device_address,
                                           const std::string& ble_address) {}

    // Batter Update message:
    // https://developers.google.com/nearby/fast-pair/spec#battery_updated
    virtual void OnBatteryUpdateMessage(
        const std::string& device_address,
        const mojom::BatteryUpdatePtr& battery_update) {}

    // Remaining Battery Time message:
    // https://developers.google.com/nearby/fast-pair/spec#battery_updated
    virtual void OnRemainingBatteryTimeMessage(
        const std::string& device_address,
        uint16_t remaining_battery_time) {}

    // Silence Mode message:
    // https://developers.google.com/nearby/fast-pair/spec#SilenceMode
    virtual void OnEnableSilenceModeMessage(const std::string& device_address,
                                            bool enable_silence_mode) {}

    // Companion App Log Buffer full message:
    // https://developers.google.com/nearby/fast-pair/spec#companion_app_events
    virtual void OnCompanionAppLogBufferFullMessage(
        const std::string& device_address) {}

    // Active components message:
    // https://developers.google.com/nearby/fast-pair/spec#MessageStreamActiveComponents
    virtual void OnActiveComponentsMessage(const std::string& device_address,
                                           uint8_t active_components_byte) {}

    // Ring device message:
    // https://developers.google.com/nearby/fast-pair/spec#ringing_a_device
    virtual void OnRingDeviceMessage(const std::string& device_address,
                                     const mojom::RingDevicePtr& ring_device) {}

    // Acknowledgement message:
    // https://developers.google.com/nearby/fast-pair/spec#MessageStreamAcknowledgements
    virtual void OnAcknowledgementMessage(
        const std::string& device_address,
        const mojom::AcknowledgementMessagePtr& acknowledgement) {}

    // Platform type message:
    // https://developers.google.com/nearby/fast-pair/spec#PlatformType
    virtual void OnAndroidSdkVersionMessage(const std::string& device_address,
                                            uint8_t sdk_version) {}

    // Observers are notified when the socket is disconnected, which means the
    // MessageStream will no longer receive new messages.
    virtual void OnDisconnected(const std::string& device_address) = 0;

    // Observers are notified when the MessageStream is being destroyed to
    // alert them to clean up their MessageStream memory objects.
    virtual void OnMessageStreamDestroyed(
        const std::string& device_address) = 0;
  };

  MessageStream(const std::string& device_address,
                scoped_refptr<device::BluetoothSocket> socket);
  MessageStream(const MessageStream&) = delete;
  MessageStream& operator=(const MessageStream&) = delete;
  ~MessageStream();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Disconnect(base::OnceClosure on_disconnect_callback);

  // Return buffer of messages.
  const base::circular_deque<mojom::MessageStreamMessagePtr>& messages() {
    return messages_;
  }

 private:
  // Attempts to receive data from socket
  void Receive();

  // Socket disconnected callbacks
  void OnSocketDisconnected();
  void OnSocketDisconnectedWithCallback(
      base::OnceClosure on_disconnect_callback);

  // Receive data from socket callbacks
  void ReceiveDataSuccess(int buffer_size,
                          scoped_refptr<net::IOBuffer> io_buffer);
  void ReceiveDataError(device::BluetoothSocket::ErrorReason error,
                        const std::string& error_message);

  // ParseMessageStreamMessage callbacks
  void ParseMessageStreamSuccess(
      std::vector<mojom::MessageStreamMessagePtr> messages);
  void OnUtilityProcessStopped(
      QuickPairProcessManager::ShutdownReason shutdown_reason);

  // Checks all fields of the union for the MessageStreamMessage type, and
  // notifies observers with the proper method based on the value stored.
  void NotifyObservers(const mojom::MessageStreamMessagePtr& message);

  std::string MessageStreamMessageTypeToString(
      const mojom::MessageStreamMessagePtr& message);

  int receive_retry_counter_ = 0;
  std::string device_address_;

  // The circular deque of messages is capped at |1000| messages, and old
  // messages will be removed from the front when new messages are added once
  // it reaches capacity.
  base::circular_deque<mojom::MessageStreamMessagePtr> messages_;
  scoped_refptr<device::BluetoothSocket> socket_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<MessageStream> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_
