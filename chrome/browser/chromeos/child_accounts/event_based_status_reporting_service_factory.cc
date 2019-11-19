// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service_factory.h"

#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
EventBasedStatusReportingService*
EventBasedStatusReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<EventBasedStatusReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
EventBasedStatusReportingServiceFactory*
EventBasedStatusReportingServiceFactory::GetInstance() {
  static base::NoDestructor<EventBasedStatusReportingServiceFactory> factory;
  return factory.get();
}

EventBasedStatusReportingServiceFactory::
    EventBasedStatusReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EventBasedStatusReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChildStatusReportingServiceFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(ScreenTimeControllerFactory::GetInstance());
}

EventBasedStatusReportingServiceFactory::
    ~EventBasedStatusReportingServiceFactory() = default;

KeyedService* EventBasedStatusReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new EventBasedStatusReportingService(context);
}

}  // namespace chromeos
