// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_KEYED_SERVICE_BATTERY_UPDATE_MESSAGE_HANDLER_H_
#define ASH_QUICK_PAIR_KEYED_SERVICE_BATTERY_UPDATE_MESSAGE_HANDLER_H_

#include <string>

#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

// Observes MessageStreams instances for devices when they are created, and
// on battery update messages, adds the battery information to the bluetooth
// device.
class BatteryUpdateMessageHandler : public MessageStreamLookup::Observer,
                                    public MessageStream::Observer {
 public:
  explicit BatteryUpdateMessageHandler(
      MessageStreamLookup* message_stream_lookup);
  BatteryUpdateMessageHandler(const BatteryUpdateMessageHandler&) = delete;
  BatteryUpdateMessageHandler& operator=(const BatteryUpdateMessageHandler&) =
      delete;
  ~BatteryUpdateMessageHandler() override;

 private:
  // MessageStreamLookup::Observer
  void OnMessageStreamConnected(const std::string& device_address,
                                MessageStream* message_stream) override;

  // MessageStream::Observer
  void OnBatteryUpdateMessage(
      const std::string& device_address,
      const mojom::BatteryUpdatePtr& battery_update) override;
  void OnDisconnected(const std::string& device_address) override;
  void OnMessageStreamDestroyed(const std::string& device_address) override;

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Parses MessageStream messages for battery update, and notifies observers
  // if it exists.
  void GetBatteryUpdateFromMessageStream(const std::string& device_address,
                                         MessageStream* message_stream);

  // Sets the battery information on the bluetooth device at |device_address|.
  void SetBatteryInfo(const std::string& device_address,
                      const mojom::BatteryUpdatePtr& battery_update);

  // Cleans up memory associated with a MessageStream corresponding to
  // |device_address| if it exists.
  void CleanUpMessageStream(const std::string& device_address);

  // Map of the classic pairing address to their corresponding MessageStreams.
  base::flat_map<std::string, raw_ptr<MessageStream, CtnExperimental>>
      message_streams_;

  scoped_refptr<device::BluetoothAdapter> adapter_;

  base::ScopedObservation<MessageStreamLookup, MessageStreamLookup::Observer>
      message_stream_lookup_observation_{this};
  base::WeakPtrFactory<BatteryUpdateMessageHandler> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_KEYED_SERVICE_BATTERY_UPDATE_MESSAGE_HANDLER_H_
