// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_

#include "ash/quick_pair/message_stream/message_stream_lookup.h"

#include <string>

#include "ash/quick_pair/message_stream/message_stream.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
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
  // device::BluetoothAdapter::Observer
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // Helper functions to create and remove message stream objects and open and
  // close RFCOMM channels based on whether the device is connected or
  // disconnected from  the adapter.
  void CreateMessageStream(device::BluetoothDevice* device);
  void RemoveMessageStream(const std::string& device_address);

  // Create RFCOMM connection callbacks.
  void OnConnected(std::string device_address,
                   scoped_refptr<device::BluetoothSocket> socket);
  void OnConnectError(const std::string& error_message);

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  base::ObserverList<MessageStreamLookup::Observer> observers_;

  base::flat_map<std::string, std::unique_ptr<MessageStream>> message_streams_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<MessageStreamLookupImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_IMPL_H_
