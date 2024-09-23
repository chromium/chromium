// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/event_based_status_reporting_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/event_based_status_reporting_service.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"

namespace ash {

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
    : ProfileKeyedServiceFactory(
          "EventBasedStatusReportingServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ChildStatusReportingServiceFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(ScreenTimeControllerFactory::GetInstance());
}

EventBasedStatusReportingServiceFactory::
    ~EventBasedStatusReportingServiceFactory() = default;

std::unique_ptr<KeyedService>
EventBasedStatusReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<EventBasedStatusReportingService>(context);
}

}  // namespace ash
