// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

// static
DeviceTrustFactory* DeviceTrustFactory::GetInstance() {
  return base::Singleton<DeviceTrustFactory>::get();
}

// static
DeviceTrustService* DeviceTrustFactory::GetForProfile(Profile* profile) {
  return static_cast<DeviceTrustService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DeviceTrustFactory::DeviceTrustFactory()
    : BrowserContextKeyedServiceFactory(
          "DeviceTrustService",
          BrowserContextDependencyManager::GetInstance()) {}

DeviceTrustFactory::~DeviceTrustFactory() = default;

KeyedService* DeviceTrustFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DeviceTrustService(static_cast<Profile*>(context));
}

}  // namespace enterprise_connectors
