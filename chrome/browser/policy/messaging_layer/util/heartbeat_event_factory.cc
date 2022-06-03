// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event_factory.h"

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace reporting {

HeartbeatEventFactory* HeartbeatEventFactory::GetInstance() {
  return base::Singleton<HeartbeatEventFactory>::get();
}

HeartbeatEventFactory::HeartbeatEventFactory()
    : BrowserContextKeyedServiceFactory(
          "HeartbeatEvent",
          BrowserContextDependencyManager::GetInstance()) {}

HeartbeatEventFactory::~HeartbeatEventFactory() = default;

KeyedService* HeartbeatEventFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HeartbeatEvent();
}

bool HeartbeatEventFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool HeartbeatEventFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace reporting
