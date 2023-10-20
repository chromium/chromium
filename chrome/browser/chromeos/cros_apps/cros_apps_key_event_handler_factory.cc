// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_key_event_handler_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/cros_apps/cros_apps_key_event_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"

// static
CrosAppsKeyEventHandlerFactory& CrosAppsKeyEventHandlerFactory::GetInstance() {
  static base::NoDestructor<CrosAppsKeyEventHandlerFactory> instance;
  return *instance;
}

CrosAppsKeyEventHandlerFactory::CrosAppsKeyEventHandlerFactory()
    : ProfileKeyedServiceFactory("CrosAppsKeyEventHandlerFactory") {}

CrosAppsKeyEventHandlerFactory::~CrosAppsKeyEventHandlerFactory() = default;

std::unique_ptr<KeyedService>
CrosAppsKeyEventHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(base::FeatureList::IsEnabled(
      chromeos::features::kCrosAppsBackgroundEventHandling));
  return std::make_unique<CrosAppsKeyEventHandler>(
      Profile::FromBrowserContext(context));
}

bool CrosAppsKeyEventHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
