// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_

#include "ash/quick_pair/message_stream/message_stream_lookup.h"

#include <string>

#include "ash/quick_pair/message_stream/message_stream.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
class BluetoothSocket;
}  // namespace device

namespace ash {
namespace quick_pair {

class MessageStreamLookupImpl : public MessageStreamLookup,
                                public device::BluetoothAdapter::Observer {
 public:
  MessageStreamLookupImpl();
  MessageStreamLookupImpl(const MessageStreamLookupImpl&) = delete;
  MessageStreamLookupImpl& operator=(const MessageStreamLookupImpl&) = delete;
  ~MessageStreamLookupImpl() override;

  void AddObserver(MessageStreamLookup::Observer* observer) override;
  void RemoveObserver(MessageStreamLookup::Observer* observer) override;

  MessageStream* GetMessageStream(const std::string& device_address) override;

 private:
  // Enum class to bind to attempts to create RFCOMM channels for logging which
  // BluetoothAdapter API triggers succeeded and failed.
  enum class CreateMessageStreamAttemptType {
    kDeviceConnectedStateChanged = 0,
    kDeviceAdded = 1,
    kDevicePairedChanged = 2,
    kDeviceChanged = 3,
  };

  // Helper function to be used in log messages to understand success and errors
  // for creating RFCOMM channel to the device.
  std::string CreateMessageStreamAttemptTypeToString(
      const CreateMessageStreamAttemptType& type);

  // device::BluetoothAdapter::Observer
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // Helper functions to create and remove message stream objects and open and
  // close RFCOMM channels based on whether the device is connected or
  // disconnected from  the adapter.
  void AttemptCreateMessageStream(const std::string& device_address,
                                  const CreateMessageStreamAttemptType& type);
  void AttemptRemoveMessageStream(const std::string& device_address);

  // Create RFCOMM connection callbacks.
  void OnConnected(std::string device_address,
                   base::TimeTicks connect_to_service_start_time,
                   const CreateMessageStreamAttemptType& type,
                   scoped_refptr<device::BluetoothSocket> socket);
  void OnConnectError(std::string device_address,
                      const CreateMessageStreamAttemptType& type,
                      const std::string& error_message);

  // Helper function to disconnect socket from a MessageStream instance and
  // destroy the MessageStream instance. Used by both |RemoveMessageStream| and
  // |DeviceRemoved|.
  void AttemptEraseMessageStream(const std::string& device_address);

  // Callback for disconnected the socket from the MessageStream.
  void OnSocketDisconnected(const std::string& device_address);

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Maps devices addresses to message stream attempt counts and retry timers,
  // respectively.
  base::flat_map<std::string, int> create_message_stream_attempts_;
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      create_message_stream_retry_timers_;

  base::ObserverList<MessageStreamLookup::Observer> observers_;

  base::flat_map<std::string, std::unique_ptr<MessageStream>> message_streams_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  base::flat_set<std::string> pending_connect_requests_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<MessageStreamLookupImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_
