// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"

namespace browser_actuator {
class TransportSession;
}  // namespace browser_actuator

namespace actor {

class ActorEnterprisePolicyChecker;
class ActorKeyedService;

// Creates ActuationTransportHandler instances for remote actuation transport
// sessions.
class ChromeActuationTransportHandlerFactory
    : public browser_actuator::TransportHandlerFactory {
 public:
  explicit ChromeActuationTransportHandlerFactory(
      ActorKeyedService* actor_keyed_service);
  ChromeActuationTransportHandlerFactory(
      const ChromeActuationTransportHandlerFactory&) = delete;
  ChromeActuationTransportHandlerFactory& operator=(
      const ChromeActuationTransportHandlerFactory&) = delete;
  ~ChromeActuationTransportHandlerFactory() override;

  // browser_actuator::TransportHandlerFactory:
  browser_actuator::FactoryId GetFactoryId() const override;
  std::vector<browser_actuator::PayloadType> GetSupportedPayloadTypes()
      const override;
  std::unique_ptr<browser_actuator::TransportHandler> OnNewSession(
      browser_actuator::TransportSession* session) override;

 private:
  // Owns `this`.
  raw_ptr<ActorKeyedService> actor_keyed_service_;
  // Default enterprise policy checker for remote actuation tasks created via
  // this factory.
  std::unique_ptr<ActorEnterprisePolicyChecker> enterprise_policy_checker_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TRANSPORT_CHROME_ACTUATION_TRANSPORT_HANDLER_FACTORY_H_
