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
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler.h"
#include "components/stream_transport/public/transport_handler_factory.h"

class Profile;

namespace stream_transport {
class TransportSession;
}  // namespace stream_transport

namespace glic {

class GlicExperimentalTriggeringCoordinator;

// TransportHandler for handling GlicExperimentalTriggering messages and
// coordinating requests over a BrowserActuator TransportSession.
class GlicExperimentalTriggeringTransportHandler
    : public stream_transport::TransportHandler {
 public:
  GlicExperimentalTriggeringTransportHandler(
      Profile* profile,
      stream_transport::TransportSession* session,
      base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator);
  ~GlicExperimentalTriggeringTransportHandler() override;

  GlicExperimentalTriggeringTransportHandler(
      const GlicExperimentalTriggeringTransportHandler&) = delete;
  GlicExperimentalTriggeringTransportHandler& operator=(
      const GlicExperimentalTriggeringTransportHandler&) = delete;

  // stream_transport::TransportHandler implementation:
  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  void SendResponse(ExperimentalTriggeringResponse response);

  const raw_ptr<Profile> profile_;
  base::WeakPtr<GlicExperimentalTriggeringCoordinator> coordinator_;
  base::WeakPtrFactory<GlicExperimentalTriggeringTransportHandler>
      weak_ptr_factory_{this};
};

class GlicExperimentalTriggeringTransportHandlerFactory
    : public stream_transport::TransportHandlerFactory {
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

  // stream_transport::TransportHandlerFactory implementation:
  stream_transport::FactoryId GetFactoryId() const override;
  std::vector<stream_transport::PayloadType> GetSupportedPayloadTypes()
      const override;
  std::unique_ptr<stream_transport::TransportHandler> OnNewSession(
      stream_transport::TransportSession* session) override;

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<GlicExperimentalTriggeringCoordinator> coordinator_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_TRANSPORT_HANDLER_H_
