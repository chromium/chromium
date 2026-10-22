// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/chrome_actuation_transport_handler_factory.h"

#include "base/check.h"
#include "chrome/browser/actor/actor_enterprise_policy_checker.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"
#include "components/actor/transport/actuation_transport_handler.h"

namespace actor {

ChromeActuationTransportHandlerFactory::ChromeActuationTransportHandlerFactory(
    ActorKeyedService* actor_keyed_service)
    : actor_keyed_service_(actor_keyed_service) {
  CHECK(actor_keyed_service_);
  enterprise_policy_checker_ = std::make_unique<ActorEnterprisePolicyChecker>(
      actor_keyed_service_->GetProfile());
}

ChromeActuationTransportHandlerFactory::
    ~ChromeActuationTransportHandlerFactory() = default;

browser_actuator::transport::FactoryId
ChromeActuationTransportHandlerFactory::GetFactoryId() const {
  return browser_actuator::transport::FactoryId::kActuation;
}

std::vector<browser_actuator::transport::PayloadType>
ChromeActuationTransportHandlerFactory::GetSupportedPayloadTypes() const {
  return {browser_actuator::transport::PayloadType::kActuation};
}

std::unique_ptr<browser_actuator::transport::TransportHandler>
ChromeActuationTransportHandlerFactory::OnNewSession(
    browser_actuator::transport::TransportSession* session) {
  return std::make_unique<ActuationTransportHandler>(
      session, std::make_unique<ActorKeyedServiceAdapter>(
                   actor_keyed_service_, enterprise_policy_checker_.get()));
}

}  // namespace actor
