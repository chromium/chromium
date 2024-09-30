// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_MESSAGE_STREAM_LOOKUP_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_MESSAGE_STREAM_LOOKUP_H_

#include <string>

#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

class MessageStream;

// Exposes a MessageStream instance to consumers for a specific device, if
// available. Observes the BluetoothAdapter for devices connected and
// disconnect and opens and closes RFCOMM channels to the device as
// appropriate, and creates and maintains MessageStream instances for each
// device.
class FakeMessageStreamLookup : public MessageStreamLookup {
 public:
  FakeMessageStreamLookup();
  FakeMessageStreamLookup(const FakeMessageStreamLookup&) = delete;
  FakeMessageStreamLookup& operator=(const FakeMessageStreamLookup&) = delete;
  ~FakeMessageStreamLookup() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  MessageStream* GetMessageStream(const std::string& device_address) override;

  void AddMessageStream(const std::string& device_address,
                        MessageStream* message_stream);
  void RemoveMessageStream(const std::string& device_address);

  void NotifyMessageStreamConnected(const std::string& device_address,
                                    MessageStream* message_stream);

 private:
  base::flat_map<std::string, raw_ptr<MessageStream, CtnExperimental>>
      message_streams_;
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_MESSAGE_STREAM_LOOKUP_H_
