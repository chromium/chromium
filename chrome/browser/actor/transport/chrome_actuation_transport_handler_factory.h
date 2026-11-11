// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler.h"
#include "components/stream_transport/public/transport_handler_factory.h"

namespace stream_transport {
class TransportSession;
}  // namespace stream_transport

namespace actor {

class ActorEnterprisePolicyChecker;
class ActorKeyedService;

// Creates ActuationTransportHandler instances for remote actuation transport
// sessions.
class ChromeActuationTransportHandlerFactory
    : public stream_transport::TransportHandlerFactory {
 public:
  explicit ChromeActuationTransportHandlerFactory(
      ActorKeyedService* actor_keyed_service);
  ChromeActuationTransportHandlerFactory(
      const ChromeActuationTransportHandlerFactory&) = delete;
  ChromeActuationTransportHandlerFactory& operator=(
      const ChromeActuationTransportHandlerFactory&) = delete;
  ~ChromeActuationTransportHandlerFactory() override;

  // stream_transport::TransportHandlerFactory:
  stream_transport::FactoryId GetFactoryId() const override;
  std::vector<stream_transport::PayloadType> GetSupportedPayloadTypes()
      const override;
  std::unique_ptr<stream_transport::TransportHandler> OnNewSession(
      stream_transport::TransportSession* session) override;

 private:
  // Owns `this`.
  raw_ptr<ActorKeyedService> actor_keyed_service_;
  // Default enterprise policy checker for remote actuation tasks created via
  // this factory.
  std::unique_ptr<ActorEnterprisePolicyChecker> enterprise_policy_checker_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_
