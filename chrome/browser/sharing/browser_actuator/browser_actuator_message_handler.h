// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "components/sharing_message/sharing_message_handler.h"

namespace stream_transport {
class TransportService;
}  // namespace stream_transport

// Handler for incoming initial SharingMessages for BrowserActuator service.
// Processes initial opt-in or trigger actuation messages to set up a
// TransportChannel and TransportSession.
class BrowserActuatorMessageHandler : public SharingMessageHandler {
 public:
  // `transport_service` must be non-null and must outlive this handler
  // (guaranteed by `SharingServiceFactory`'s dependency on
  // `TransportServiceFactory`).
  explicit BrowserActuatorMessageHandler(
      stream_transport::TransportService* transport_service);
  BrowserActuatorMessageHandler(const BrowserActuatorMessageHandler&) = delete;
  BrowserActuatorMessageHandler& operator=(
      const BrowserActuatorMessageHandler&) = delete;
  ~BrowserActuatorMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

 private:
  const raw_ref<stream_transport::TransportService> transport_service_;
};

#endif  // CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
