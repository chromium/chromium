// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event_factory.h"

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace reporting {

ManualTestHeartbeatEventFactory*
ManualTestHeartbeatEventFactory::GetInstance() {
  return base::Singleton<ManualTestHeartbeatEventFactory>::get();
}

ManualTestHeartbeatEventFactory::ManualTestHeartbeatEventFactory()
    : BrowserContextKeyedServiceFactory(
          "ManualTestHeartbeatEvent",
          BrowserContextDependencyManager::GetInstance()) {}

ManualTestHeartbeatEventFactory::~ManualTestHeartbeatEventFactory() = default;

KeyedService* ManualTestHeartbeatEventFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ManualTestHeartbeatEvent();
}

bool ManualTestHeartbeatEventFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ManualTestHeartbeatEventFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace reporting
