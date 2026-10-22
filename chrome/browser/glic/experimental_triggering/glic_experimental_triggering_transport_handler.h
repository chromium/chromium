// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory.h"

class Profile;

namespace browser_actuator::transport {
class TransportSession;
}  // namespace browser_actuator::transport

namespace glic {

class GlicExperimentalTriggeringCoordinator;

// TransportHandler for handling GlicExperimentalTriggering messages and
// coordinating requests over a BrowserActuator TransportSession.
class GlicExperimentalTriggeringTransportHandler
    : public browser_actuator::transport::TransportHandler {
 public:
  GlicExperimentalTriggeringTransportHandler(
      Profile* profile,
      browser_actuator::transport::TransportSession* session,
      base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator);
  ~GlicExperimentalTriggeringTransportHandler() override;

  GlicExperimentalTriggeringTransportHandler(
      const GlicExperimentalTriggeringTransportHandler&) = delete;
  GlicExperimentalTriggeringTransportHandler& operator=(
      const GlicExperimentalTriggeringTransportHandler&) = delete;

  // browser_actuator::transport::TransportHandler implementation:
  void OnMessage(browser_actuator::transport::PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  void SendResponse(ExperimentalTriggeringResponse response);

  const raw_ptr<Profile> profile_;
  base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtrFactory<GlicExperimentalTriggeringTransportHandler>
      weak_ptr_factory_{this};
};

class GlicExperimentalTriggeringTransportHandlerFactory
    : public browser_actuator::transport::TransportHandlerFactory {
 public:
  explicit GlicExperimentalTriggeringTransportHandlerFactory(Profile* profile);
  GlicExperimentalTriggeringTransportHandlerFactory(
      Profile* profile,
      std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator);
  ~GlicExperimentalTriggeringTransportHandlerFactory() override;

  GlicExperimentalTriggeringTransportHandlerFactory(
      const GlicExperimentalTriggeringTransportHandlerFactory&) = delete;
  GlicExperimentalTriggeringTransportHandlerFactory& operator=(
      const GlicExperimentalTriggeringTransportHandlerFactory&) = delete;

  // browser_actuator::transport::TransportHandlerFactory implementation:
  browser_actuator::transport::FactoryId GetFactoryId() const override;
  std::vector<browser_actuator::transport::PayloadType>
  GetSupportedPayloadTypes() const override;
  std::unique_ptr<browser_actuator::transport::TransportHandler> OnNewSession(
      browser_actuator::transport::TransportSession* session) override;

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
