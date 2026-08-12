// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"

namespace browser_actuator {
class TransportSession;
}

namespace glic {

class GlicExperimentalOptInController;

// TransportHandler for handling GlicExperimentalTriggering messages and opt-in
// requests.
class GlicExperimentalTriggeringTransportHandler
    : public browser_actuator::TransportHandler {
 public:
  GlicExperimentalTriggeringTransportHandler(
      GlicExperimentalOptInController* opt_in_controller,
      browser_actuator::TransportSession* session);
  ~GlicExperimentalTriggeringTransportHandler() override;

  GlicExperimentalTriggeringTransportHandler(
      const GlicExperimentalTriggeringTransportHandler&) = delete;
  GlicExperimentalTriggeringTransportHandler& operator=(
      const GlicExperimentalTriggeringTransportHandler&) = delete;

  // browser_actuator::TransportHandler implementation:
  void OnMessage(const google::protobuf::MessageLite& message) override;

  // Handles opt-in request directly from sharing message or other callers.
  void HandleOptInRequest(
      const components_sharing_message::GlicExperimentalTriggering& triggering);

 private:
  void OnOptInCompleted(
      const components_sharing_message::GlicExperimentalTriggering& request,
      bool accepted);

  void SendOptInResponse(
      const components_sharing_message::GlicExperimentalTriggering& request,
      components_sharing_message::GlicExperimentalTriggering::
          ExperimentalTriggeringResponse::DeviceOptInResult result);

  const raw_ptr<GlicExperimentalOptInController> opt_in_controller_ = nullptr;
  const raw_ptr<browser_actuator::TransportSession> session_;
  base::WeakPtrFactory<GlicExperimentalTriggeringTransportHandler>
      weak_ptr_factory_{this};
};

class GlicExperimentalTriggeringTransportHandlerFactory
    : public browser_actuator::TransportHandlerFactory {
 public:
  explicit GlicExperimentalTriggeringTransportHandlerFactory(
      GlicExperimentalOptInController* opt_in_controller);
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
  const raw_ptr<GlicExperimentalOptInController> opt_in_controller_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
