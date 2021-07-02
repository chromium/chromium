// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event_factory.h"

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_chromeos.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

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
  Profile* profile = static_cast<Profile*>(context);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserCloudPolicyManagerChromeOS* manager =
      profile->GetUserCloudPolicyManagerChromeOS();
#else
  policy::UserCloudPolicyManager* manager =
      profile->GetUserCloudPolicyManager();
#endif

  if (!manager) {
    return nullptr;
  }
  return new HeartbeatEvent(manager);
}

bool HeartbeatEventFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool HeartbeatEventFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace reporting
