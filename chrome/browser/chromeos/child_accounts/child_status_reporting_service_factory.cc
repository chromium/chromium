// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service_factory.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
ChildStatusReportingService*
ChildStatusReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChildStatusReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChildStatusReportingServiceFactory*
ChildStatusReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ChildStatusReportingServiceFactory> factory;
  return factory.get();
}

ChildStatusReportingServiceFactory::ChildStatusReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ChildStatusReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ChildStatusReportingServiceFactory::~ChildStatusReportingServiceFactory() =
    default;

KeyedService* ChildStatusReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChildStatusReportingService(context);
}

}  // namespace chromeos
