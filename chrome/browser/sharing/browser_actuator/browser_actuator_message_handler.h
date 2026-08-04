// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_

#include <memory>

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

namespace glic {
class GlicExperimentalTriggeringCoordinator;
}

// Handler for incoming initial SharingMessages for BrowserActuator service.
// Inherits from GlicExperimentalTriggeringMessageHandler to reuse all initial
// message validation, logging, and coordinator request processing.
// Does not send status updates back over the sharing message channel.
class BrowserActuatorMessageHandler
    : public GlicExperimentalTriggeringMessageHandler {
 public:
  explicit BrowserActuatorMessageHandler(Profile* profile);
  BrowserActuatorMessageHandler(
      Profile* profile,
      std::unique_ptr<glic::GlicExperimentalTriggeringCoordinator> coordinator);
  BrowserActuatorMessageHandler(const BrowserActuatorMessageHandler&) = delete;
  BrowserActuatorMessageHandler& operator=(
      const BrowserActuatorMessageHandler&) = delete;
  ~BrowserActuatorMessageHandler() override;

 protected:
  // GlicExperimentalTriggeringMessageHandler overrides:
  void CheckFeatureFlags() const override;
  glic::GlicExperimentalTriggeringUpdateCallback GetUpdateCallback(
      components_sharing_message::SharingMessage& message) override;
};

#endif  // CHROME_BROWSER_SHARING_BROWSER_ACTUATOR_BROWSER_ACTUATOR_MESSAGE_HANDLER_H_
