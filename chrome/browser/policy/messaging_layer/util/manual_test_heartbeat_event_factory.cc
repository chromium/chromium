// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event_factory.h"

#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event.h"

namespace reporting {

ManualTestHeartbeatEventFactory*
ManualTestHeartbeatEventFactory::GetInstance() {
  static base::NoDestructor<ManualTestHeartbeatEventFactory> instance;
  return instance.get();
}

ManualTestHeartbeatEventFactory::ManualTestHeartbeatEventFactory()
    : ProfileKeyedServiceFactory(
          "ManualTestHeartbeatEvent",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ManualTestHeartbeatEventFactory::~ManualTestHeartbeatEventFactory() = default;

std::unique_ptr<KeyedService>
ManualTestHeartbeatEventFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ManualTestHeartbeatEvent>();
}

bool ManualTestHeartbeatEventFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ManualTestHeartbeatEventFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace reporting
