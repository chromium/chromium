// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "base/containers/contains.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace {

const device::BluetoothUUID kMessageStreamUuid(
    "df21fe2c-2515-4fdb-8886-f12c4d67927c");
constexpr int kMaxCreateMessageStreamAttempts{6};

// Attempt retry `n` after cooldown period |message_retry_cooldowns[n-1]|.
// These cooldown periods replicate those that Android's Fast Pair service
// mandates.
const std::vector<base::TimeDelta> kCreateMessageStreamRetryCooldowns{
    base::Seconds(2), base::Seconds(4), base::Seconds(8), base::Seconds(16),
    base::Seconds(32)};

}  // namespace

namespace ash {
namespace quick_pair {

std::string MessageStreamLookupImpl::CreateMessageStreamAttemptTypeToString(
    const CreateMessageStreamAttemptType& type) {
  switch (type) {
    case CreateMessageStreamAttemptType::kDeviceConnectedStateChanged:
      return "[DeviceConnectedStateChanged]";
    case CreateMessageStreamAttemptType::kDeviceAdded:
      return "[DeviceAdded]";
    case CreateMessageStreamAttemptType::kDevicePairedChanged:
      return "[DevicePairedChanged]";
    case CreateMessageStreamAttemptType::kDeviceChanged:
      return "[DeviceChanged]";
  }

  NOTREACHED();
}

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

void MessageStreamLookupImpl::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  // This event is triggered for all paired devices when BT is toggled on, so it
  // is important to make sure the device is actively connected or a connection
  // attempt will be issued for the Message Stream service UUID which prevents
  // audio profiles from connecting.
  if (!device->IsConnected()) {
    return;
  }

  // Check to see if the device supports Message Streams.
  if (!device || !base::Contains(device->GetUUIDs(), kMessageStreamUuid)) {
    return;
  }

  // Remove and delete the memory stream for the device, if it exists.
  if (!new_paired_status) {
    AttemptRemoveMessageStream(device->GetAddress());
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Attempting to create MessageStream for device = ["
      << device->GetAddress() << "] " << device->GetNameForDisplay();
  AttemptCreateMessageStream(
      device->GetAddress(),
      CreateMessageStreamAttemptType::kDevicePairedChanged);
}

void MessageStreamLookupImpl::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  // Check to see if the device supports Message Streams.
  if (!device || !device->IsPaired() ||
      !base::Contains(device->GetUUIDs(), kMessageStreamUuid)) {
    return;
  }

  // Remove and delete the memory stream for the device, if it exists.
  if (!is_now_connected) {
    AttemptRemoveMessageStream(device->GetAddress());
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Attempting to create MessageStream for device = ["
      << device->GetAddress() << "] " << device->GetNameForDisplay();
  AttemptCreateMessageStream(
      device->GetAddress(),
      CreateMessageStreamAttemptType::kDeviceConnectedStateChanged);
}

void MessageStreamLookupImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  // Check to see if the device is connected and supports MessageStreams. We
  // need to check if the device is both connected and paired to the adapter
  // because it is possible for a device to be connected to the adapter but not
  // paired (example: a request for the adapter's SDP records).
  if (!device || !(device->IsConnected() && device->IsPaired()) ||
      !base::Contains(device->GetUUIDs(), kMessageStreamUuid)) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": found connected device. Attempting to create "
         "MessageStream for device = ["
      << device->GetAddress() << "] " << device->GetNameForDisplay();
  AttemptCreateMessageStream(device->GetAddress(),
                             CreateMessageStreamAttemptType::kDeviceChanged);
}

void MessageStreamLookupImpl::DeviceAdded(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  // Check to see if the device is connected and supports MessageStreams. We
  // need to check if the device is both connected and paired to the adapter
  // because it is possible for a device to be connected to the adapter but not
  // paired (example: a request for the adapter's SDP records).
  if (!device || !(device->IsConnected() && device->IsPaired()) ||
      !base::Contains(device->GetUUIDs(), kMessageStreamUuid)) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": found connected device. Attempting to create "
         "MessageStream for device = ["
      << device->GetAddress() << "] " << device->GetNameForDisplay();
  AttemptCreateMessageStream(device->GetAddress(),
                             CreateMessageStreamAttemptType::kDeviceAdded);
}

void MessageStreamLookupImpl::DeviceRemoved(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  if (!device)
    return;

  // Remove message stream if the device removed from the adapter has a
  // message stream and disconnect from socket if applicable. It isn't expected
  // to already have a MessageStream associated with it.
  AttemptEraseMessageStream(device->GetAddress());
}

void MessageStreamLookupImpl::AttemptRemoveMessageStream(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": device address = " << device_address;
  AttemptEraseMessageStream(device_address);
}

void MessageStreamLookupImpl::AttemptEraseMessageStream(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": device address = " << device_address;
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

void MessageStreamLookupImpl::AttemptCreateMessageStream(
    const std::string& device_address,
    const CreateMessageStreamAttemptType& type) {
  device::BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": lost device for Message Stream creation";
    AttemptRemoveMessageStream(device_address);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": device address = " << device_address
      << " type = " << CreateMessageStreamAttemptTypeToString(type);

  // Only open MessageStreams for new devices that don't already have a
  // MessageStream stored in the map. We can sometimes reach this point if
  // multiple BluetoothAdapter events fire for a device connected event, but
  // we need all of these BluetoothAdapter observation events to handle
  // different connection scenarios, and have coverage for different devices.
  if (base::Contains(message_streams_, device_address)) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Message Stream exists already for device";
    return;
  }

  if (base::Contains(pending_connect_requests_, device_address)) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Ignoring due to matching pending request";
    return;
  }

  pending_connect_requests_.insert(device_address);

  device->ConnectToService(
      /*uuid=*/kMessageStreamUuid, /*callback=*/
      base::BindOnce(&MessageStreamLookupImpl::OnConnected,
                     weak_ptr_factory_.GetWeakPtr(), device_address,
                     base::TimeTicks::Now(), type),
      /*error_callback=*/
      base::BindOnce(&MessageStreamLookupImpl::OnConnectError,
                     weak_ptr_factory_.GetWeakPtr(), device_address, type));
}

void MessageStreamLookupImpl::OnConnected(
    std::string device_address,
    base::TimeTicks connect_to_service_start_time,
    const CreateMessageStreamAttemptType& type,
    scoped_refptr<device::BluetoothSocket> socket) {
  if (create_message_stream_retry_timers_.contains(device_address)) {
    base::OneShotTimer* curr_create_message_stream_retry_timer =
        create_message_stream_retry_timers_[device_address].get();

    // This if branch should be unnecessary in theory, but it is included to
    // address the edge case that a success occurs after a failure.
    if (curr_create_message_stream_retry_timer->IsRunning())
      curr_create_message_stream_retry_timer->Stop();

    size_t timer_erased_ct =
        create_message_stream_retry_timers_.erase(device_address);
    DCHECK(timer_erased_ct == 1);
    size_t retry_ct_erased_ct =
        create_message_stream_attempts_.erase(device_address);
    DCHECK(retry_ct_erased_ct == 1);
  }

  // It is expected that at the point of a successful RFCOMM connection, the
  // device is known to the adapter.
  device::BluetoothDevice* bt_device = adapter_->GetDevice(device_address);
  DCHECK(bt_device);
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": device = " << device_address
      << " device name = " << bt_device->GetNameForDisplay()
      << " Type = " << CreateMessageStreamAttemptTypeToString(type);

  RecordMessageStreamConnectToServiceResult(/*success=*/true);
  RecordMessageStreamConnectToServiceTime(base::TimeTicks::Now() -
                                          connect_to_service_start_time);

  std::unique_ptr<MessageStream> message_stream =
      std::make_unique<MessageStream>(device_address, std::move(socket));

  for (auto& observer : observers_)
    observer.OnMessageStreamConnected(device_address, message_stream.get());

  message_streams_[device_address] = std::move(message_stream);
  pending_connect_requests_.erase(device_address);
}

void MessageStreamLookupImpl::OnConnectError(
    std::string device_address,
    const CreateMessageStreamAttemptType& type,
    const std::string& error_message) {
  // Because we need to attempt to create MessageStreams at many different
  // iterations due to the variability of Bluetooth APIs, we can expect to
  // see errors here frequently, along with errors followed by a success.
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Error: [ " << error_message
      << "]. Type: " << CreateMessageStreamAttemptTypeToString(type) << ".";
  RecordMessageStreamConnectToServiceResult(/*success=*/false);
  RecordMessageStreamConnectToServiceError(error_message);
  pending_connect_requests_.erase(device_address);

  // A timer is started to retry AttemptCreateMessageStream if
  // the maximum number of attempts (6) to create the MessageStream has not been
  // reached. If this is the first retry, new entries in
  // |create_message_stream_attempts_| and
  // |create_message_stream_retry_timers_| are created.
  create_message_stream_attempts_.try_emplace(device_address, 1);

  int& create_message_stream_attempt_num =
      create_message_stream_attempts_[device_address];
  if (create_message_stream_attempt_num == kMaxCreateMessageStreamAttempts) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": 6 attempts to create a message stream have failed. "
           "There are no more retries.";
    return;
  }

  device::BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (device) {
    create_message_stream_retry_timers_.try_emplace(
        device_address, std::make_unique<base::OneShotTimer>());

    base::OneShotTimer* curr_create_message_stream_retry_timer =
        create_message_stream_retry_timers_[device_address].get();
    curr_create_message_stream_retry_timer->Start(
        FROM_HERE,
        kCreateMessageStreamRetryCooldowns[create_message_stream_attempt_num++ -
                                           1],
        base::BindOnce(&MessageStreamLookupImpl::AttemptCreateMessageStream,
                       weak_ptr_factory_.GetWeakPtr(), device_address, type));
  } else {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": attempting to retry message stream creation with "
        << " a device no longer found by the adapter."
        << " device address: " << device_address;
    size_t retry_ct_erased_ct =
        create_message_stream_attempts_.erase(device_address);
    DCHECK(retry_ct_erased_ct == 1);
    create_message_stream_retry_timers_.erase(device_address);
  }
}

}  // namespace quick_pair
}  // namespace ash
