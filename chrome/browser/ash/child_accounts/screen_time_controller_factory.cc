// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"

namespace ash {

// static
ScreenTimeController* ScreenTimeControllerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ScreenTimeController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ScreenTimeControllerFactory* ScreenTimeControllerFactory::GetInstance() {
  static base::NoDestructor<ScreenTimeControllerFactory> factory;
  return factory.get();
}

ScreenTimeControllerFactory::ScreenTimeControllerFactory()
    : ProfileKeyedServiceFactory(
          "ScreenTimeControllerFactory",
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
}

ScreenTimeControllerFactory::~ScreenTimeControllerFactory() = default;

std::unique_ptr<KeyedService>
ScreenTimeControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ScreenTimeController>(context);
}

}  // namespace ash
