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
    : ProfileKeyedServiceFactory("ScreenTimeControllerFactory") {
  DependsOn(ChildStatusReportingServiceFactory::GetInstance());
}

ScreenTimeControllerFactory::~ScreenTimeControllerFactory() = default;

KeyedService* ScreenTimeControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ScreenTimeController(context);
}

}  // namespace ash
