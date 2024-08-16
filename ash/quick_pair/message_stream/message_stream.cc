// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/quick_pair/message_stream/message_stream.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace {

constexpr int kMaxBufferSize = 4096;
constexpr int kMaxRetryCount = 10;
constexpr int kMessageStorageCapacity = 1000;

}  // namespace

namespace ash {
namespace quick_pair {

MessageStream::MessageStream(const std::string& device_address,
                             scoped_refptr<device::BluetoothSocket> socket)
    : device_address_(device_address), socket_(socket) {
  Receive();
}

MessageStream::~MessageStream() {
  if (socket_.get())
    socket_->Disconnect(base::DoNothing());

  // Notify observers for lifetime management
  for (auto& obs : observers_)
    obs.OnMessageStreamDestroyed(device_address_);
}

void MessageStream::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageStream::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessageStream::Receive() {
  if (receive_retry_counter_ == kMaxRetryCount) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to receive or parse data from socket more than "
        << kMaxRetryCount << " times.";

    if (socket_.get()) {
      socket_->Disconnect(base::BindOnce(&MessageStream::OnSocketDisconnected,
                                         weak_ptr_factory_.GetWeakPtr()));
    }

    return;
  }

  // Retry receiving data.
  receive_retry_counter_++;
  socket_->Receive(/*buffer_size=*/kMaxBufferSize,
                   base::BindOnce(&MessageStream::ReceiveDataSuccess,
                                  weak_ptr_factory_.GetWeakPtr()),
                   base::BindOnce(&MessageStream::ReceiveDataError,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void MessageStream::ReceiveDataSuccess(int buffer_size,
                                       scoped_refptr<net::IOBuffer> io_buffer) {
  RecordMessageStreamReceiveResult(/*success=*/true);
  receive_retry_counter_ = 0;

  if (!io_buffer->data()) {
    Receive();
    return;
  }

  std::vector<uint8_t> message_bytes(buffer_size);
  for (int i = 0; i < buffer_size; i++) {
    char* c = io_buffer->data() + i;
    message_bytes[i] = static_cast<uint8_t>(*c);
  }

  quick_pair_process::ParseMessageStreamMessages(
      std::move(message_bytes),
      base::BindOnce(&MessageStream::ParseMessageStreamSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MessageStream::OnUtilityProcessStopped,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MessageStream::ReceiveDataError(device::BluetoothSocket::ErrorReason error,
                                     const std::string& error_message) {
  CD_LOG(INFO, Feature::FP) << __func__ << ": Error: " << error_message;
  RecordMessageStreamReceiveResult(/*success=*/false);
  RecordMessageStreamReceiveError(error);

  if (error == device::BluetoothSocket::ErrorReason::kDisconnected) {
    OnSocketDisconnected();
    return;
  }

  Receive();
}

void MessageStream::Disconnect(base::OnceClosure on_disconnect_callback) {
  CD_LOG(INFO, Feature::FP) << __func__;

  // If we already have disconnected the socket, then we can run the callback.
  // This can happen since the socket might have disconnected previously but
  // we kept the MessageStream instance alive to preserve messages from the
  // corresponding device.
  if (!socket_.get()) {
    std::move(on_disconnect_callback).Run();
    return;
  }

  socket_->Disconnect(base::BindOnce(
      &MessageStream::OnSocketDisconnectedWithCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_disconnect_callback)));
}

void MessageStream::OnSocketDisconnected() {
  for (auto& obs : observers_)
    obs.OnDisconnected(device_address_);
}

void MessageStream::OnSocketDisconnectedWithCallback(
    base::OnceClosure on_disconnect_callback) {
  OnSocketDisconnected();
  std::move(on_disconnect_callback).Run();
}

void MessageStream::ParseMessageStreamSuccess(
    std::vector<mojom::MessageStreamMessagePtr> messages) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  if (messages.empty()) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": no messages";
    Receive();
    return;
  }

  // Store messages and notify observers.
  for (size_t i = 0; i < messages.size(); ++i) {
    if (messages_.size() == kMessageStorageCapacity)
      messages_.pop_front();

    messages_.push_back(std::move(messages[i]));
    NotifyObservers(messages_.back());
  }

  // Attempt to receive new messages from socket.
  Receive();
}

std::string MessageStream::MessageStreamMessageTypeToString(
    const mojom::MessageStreamMessagePtr& message) {
  if (message->is_model_id())
    return "Model ID";

  if (message->is_ble_address_update())
    return "BLE address update";

  if (message->is_battery_update())
    return "Battery Update";

  if (message->is_remaining_battery_time())
    return "Remaining Battery Time";

  if (message->is_enable_silence_mode())
    return "Enable Silence Mode";

  if (message->is_companion_app_log_buffer_full())
    return "Companion App Log Buffer Full";

  if (message->is_active_components_byte())
    return "Active Components Byte";

  if (message->is_ring_device_event())
    return "Ring Device Event";

  if (message->is_acknowledgement())
    return "Acknowledgement";

  if (message->is_sdk_version())
    return "SDK version";

  NOTREACHED();
}

void MessageStream::NotifyObservers(
    const mojom::MessageStreamMessagePtr& message) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": MessageStreamMessagePtr is "
                               << MessageStreamMessageTypeToString(message);

  if (message->is_model_id()) {
    for (auto& obs : observers_)
      obs.OnModelIdMessage(device_address_, message->get_model_id());

    return;
  }

  if (message->is_ble_address_update()) {
    for (auto& obs : observers_)
      obs.OnBleAddressUpdateMessage(device_address_,
                                    message->get_ble_address_update());

    return;
  }

  if (message->is_battery_update()) {
    for (auto& obs : observers_)
      obs.OnBatteryUpdateMessage(device_address_,
                                 std::move(message->get_battery_update()));

    return;
  }

  if (message->is_remaining_battery_time()) {
    for (auto& obs : observers_)
      obs.OnRemainingBatteryTimeMessage(device_address_,
                                        message->get_remaining_battery_time());

    return;
  }

  if (message->is_enable_silence_mode()) {
    for (auto& obs : observers_)
      obs.OnEnableSilenceModeMessage(device_address_,
                                     message->get_enable_silence_mode());

    return;
  }

  if (message->is_companion_app_log_buffer_full()) {
    for (auto& obs : observers_)
      obs.OnCompanionAppLogBufferFullMessage(device_address_);

    return;
  }

  if (message->is_active_components_byte()) {
    for (auto& obs : observers_)
      obs.OnActiveComponentsMessage(device_address_,
                                    message->get_active_components_byte());

    return;
  }

  if (message->is_ring_device_event()) {
    for (auto& obs : observers_)
      obs.OnRingDeviceMessage(device_address_,
                              std::move(message->get_ring_device_event()));

    return;
  }

  if (message->is_acknowledgement()) {
    for (auto& obs : observers_)
      obs.OnAcknowledgementMessage(device_address_,
                                   std::move(message->get_acknowledgement()));

    return;
  }

  if (message->is_sdk_version()) {
    for (auto& obs : observers_)
      obs.OnAndroidSdkVersionMessage(device_address_,
                                     message->get_sdk_version());

    return;
  }

  CD_LOG(WARNING, Feature::FP) << __func__ << ": unexpected message type.";
  NOTREACHED();
}

void MessageStream::OnUtilityProcessStopped(
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  CD_LOG(INFO, Feature::FP) << __func__ << ": Error: " << shutdown_reason;

  receive_retry_counter_++;
  Receive();
}

}  // namespace quick_pair
}  // namespace ash
