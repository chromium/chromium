// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "base/containers/contains.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace {

const device::BluetoothUUID kMessageStreamUuid(
    "df21fe2c-2515-4fdb-8886-f12c4d67927c");

}  // namespace

namespace ash {
namespace quick_pair {

MessageStreamLookupImpl::MessageStreamLookupImpl() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &MessageStreamLookupImpl::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

void MessageStreamLookupImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());
}

void MessageStreamLookupImpl::AddObserver(
    MessageStreamLookup::Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageStreamLookupImpl::RemoveObserver(
    MessageStreamLookup::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MessageStreamLookupImpl::~MessageStreamLookupImpl() = default;

MessageStream* MessageStreamLookupImpl::GetMessageStream(
    const std::string& device_address) {
  auto it = message_streams_.find(device_address);
  // If we don't have a MessageStream for the device at |device_address|, return
  // a nullptr.
  if (it == message_streams_.end())
    return nullptr;

  // Return the pointer underneath the unique_ptr to the MessageStream we are
  // owning for the device at |device_address|.
  return it->second.get();
}

void MessageStreamLookupImpl::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  // Check to see if the device supports Message Streams.
  if (!base::Contains(device->GetUUIDs(), kMessageStreamUuid))
    return;

  // Remove and delete the memory stream for the device, if it exists.
  if (!is_now_connected) {
    RemoveMessageStream(device->GetAddress());
    return;
  }

  CreateMessageStream(device);
}

void MessageStreamLookupImpl::DeviceRemoved(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  if (!device)
    return;

  // Remove message stream if the device removed from the adapter has a
  // message stream and disconnect from socket if applicable. It isn't expected
  // to already have a MessageStream associated with it.
  EraseMessageStream(device->GetAddress());
}

void MessageStreamLookupImpl::RemoveMessageStream(
    const std::string& device_address) {
  QP_LOG(VERBOSE) << __func__ << ": device address = " << device_address;
  EraseMessageStream(device_address);
}

void MessageStreamLookupImpl::EraseMessageStream(
    const std::string& device_address) {
  // Remove map entry if it exists. It may not exist if it was failed to be
  // created due to a |ConnectToService| error.
  if (!base::Contains(message_streams_, device_address))
    return;

  // If the MessageStream still exists, we can attempt to gracefully disconnect
  // the socket before erasing (and therefore destructing) the MessageStream
  // instance.
  message_streams_[device_address]->Disconnect(
      base::BindOnce(&MessageStreamLookupImpl::OnSocketDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), device_address));
}

void MessageStreamLookupImpl::OnSocketDisconnected(
    const std::string& device_address) {
  message_streams_.erase(device_address);
}

void MessageStreamLookupImpl::CreateMessageStream(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": device address = " << device->GetAddress();

  // Only open message streams for new devices that don't already have a
  // message stream stored in the map.
  const std::string& device_address = device->GetAddress();
  DCHECK(message_streams_.find(device_address) == message_streams_.end());

  device->ConnectToService(
      /*uuid=*/kMessageStreamUuid, /*callback=*/
      base::BindOnce(&MessageStreamLookupImpl::OnConnected,
                     weak_ptr_factory_.GetWeakPtr(), device_address,
                     base::TimeTicks::Now()),
      /*error_callback=*/
      base::BindOnce(&MessageStreamLookupImpl::OnConnectError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MessageStreamLookupImpl::OnConnected(
    std::string device_address,
    base::TimeTicks connect_to_service_start_time,
    scoped_refptr<device::BluetoothSocket> socket) {
  QP_LOG(VERBOSE) << __func__;
  RecordMessageStreamConnectToServiceResult(/*success=*/true);
  RecordMessageStreamConnectToServiceTime(base::TimeTicks::Now() -
                                          connect_to_service_start_time);

  std::unique_ptr<MessageStream> message_stream =
      std::make_unique<MessageStream>(device_address, std::move(socket));

  for (auto& observer : observers_)
    observer.OnMessageStreamConnected(device_address, message_stream.get());

  message_streams_[device_address] = std::move(message_stream);
}

void MessageStreamLookupImpl::OnConnectError(const std::string& error_message) {
  QP_LOG(INFO) << __func__ << ": Error = [ " << error_message << "].";
  RecordMessageStreamConnectToServiceResult(/*success=*/false);
  RecordMessageStreamConnectToServiceError(error_message);
}

}  // namespace quick_pair
}  // namespace ash
