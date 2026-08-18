// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"

class Profile;

namespace browser_actuator {
class TransportSession;
}

namespace glic {

class GlicExperimentalTriggeringCoordinator;

// TransportHandler for handling GlicExperimentalTriggering messages and
// coordinating requests over a BrowserActuator TransportSession.
class GlicExperimentalTriggeringTransportHandler
    : public browser_actuator::TransportHandler {
 public:
  GlicExperimentalTriggeringTransportHandler(
      Profile* profile,
      browser_actuator::TransportSession* session,
      std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator);
  ~GlicExperimentalTriggeringTransportHandler() override;

  GlicExperimentalTriggeringTransportHandler(
      const GlicExperimentalTriggeringTransportHandler&) = delete;
  GlicExperimentalTriggeringTransportHandler& operator=(
      const GlicExperimentalTriggeringTransportHandler&) = delete;

  // browser_actuator::TransportHandler implementation:
  void OnMessage(const google::protobuf::MessageLite& message) override;

 private:
  void SendResponse(ExperimentalTriggeringResponse response);

  const raw_ptr<Profile> profile_;
  const raw_ptr<browser_actuator::TransportSession> session_;
  std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtrFactory<GlicExperimentalTriggeringTransportHandler>
      weak_ptr_factory_{this};
};

class GlicExperimentalTriggeringTransportHandlerFactory
    : public browser_actuator::TransportHandlerFactory {
 public:
  explicit GlicExperimentalTriggeringTransportHandlerFactory(Profile* profile);
  ~GlicExperimentalTriggeringTransportHandlerFactory() override;

  GlicExperimentalTriggeringTransportHandlerFactory(
      const GlicExperimentalTriggeringTransportHandlerFactory&) = delete;
  GlicExperimentalTriggeringTransportHandlerFactory& operator=(
      const GlicExperimentalTriggeringTransportHandlerFactory&) = delete;

  // browser_actuator::TransportHandlerFactory implementation:
  browser_actuator::FactoryId GetFactoryId() const override;
  std::vector<browser_actuator::PayloadType> GetSupportedPayloadTypes()
      const override;
  std::unique_ptr<browser_actuator::TransportHandler> OnNewSession(
      browser_actuator::TransportSession* session) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
