// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_message_handler.h"

class Profile;

namespace components_sharing_message {
class GlicExperimentalTriggering;
}  // namespace components_sharing_message

// Handler for incoming initial SharingMessages for BrowserActuator service.
// Processes initial opt-in messages to set up a TransportChannel and
// TransportSession.
class BrowserActuatorMessageHandler : public SharingMessageHandler {
 public:
  explicit BrowserActuatorMessageHandler(Profile* profile);
  BrowserActuatorMessageHandler(const BrowserActuatorMessageHandler&) = delete;
  BrowserActuatorMessageHandler& operator=(
      const BrowserActuatorMessageHandler&) = delete;
  ~BrowserActuatorMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 DoneCallback done_callback) override;

 private:
  void HandleGlicExperimentalTriggering(
      const components_sharing_message::GlicExperimentalTriggering& triggering);

  // Helper to initialize/get the TransportSession for
  // session_id.
  void EnsureTransportSessionCreated(const std::string& session_id);

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
