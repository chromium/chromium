// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_H_

#include <string>

#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

class MessageStream;

// Exposes a MessageStream instance to consumers for a specific device, if
// available. Observes the BluetoothAdapter for devices connected and
// disconnect and opens and closes RFCOMM channels to the device as
// appropriate, and creates and maintains MessageStream instances for each
// device.
class MessageStreamLookup {
 public:
  // For immediate consumption of MessageStream upon creation, use the
  // observer model to retrieve the MessageStream when the socket connects.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMessageStreamConnected(const std::string& device_address,
                                          MessageStream* message_stream) = 0;
  };

  virtual ~MessageStreamLookup() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // To retrieve an existing instance of a MessageStream, retrieve an instance
  // this way.
  virtual MessageStream* GetMessageStream(
      const std::string& device_address) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_MESSAGE_STREAM_LOOKUP_H_
