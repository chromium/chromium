// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/dips/dips_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace dips {

// static
DIPSService* DIPSServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSServiceFactory* DIPSServiceFactory::GetInstance() {
  return base::Singleton<DIPSServiceFactory>::get();
}

DIPSServiceFactory::DIPSServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DIPSService",
          BrowserContextDependencyManager::GetInstance()) {}

DIPSServiceFactory::~DIPSServiceFactory() = default;

KeyedService* DIPSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSService(context);
}

}  // namespace dips
