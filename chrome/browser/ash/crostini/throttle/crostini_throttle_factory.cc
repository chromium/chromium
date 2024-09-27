// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_throttle_factory.h"

#include "chrome/browser/ash/crostini/throttle/crostini_throttle.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniThrottle* CrostiniThrottleFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CrostiniThrottle*>(
      CrostiniThrottleFactory::GetInstance()->GetServiceForBrowserContext(
          context, true /* create */));
}

// static
CrostiniThrottleFactory* CrostiniThrottleFactory::GetInstance() {
  static base::NoDestructor<CrostiniThrottleFactory> instance;
  return instance.get();
}

CrostiniThrottleFactory::CrostiniThrottleFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniThrottleFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CrostiniThrottleFactory::~CrostiniThrottleFactory() = default;

KeyedService* CrostiniThrottleFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new CrostiniThrottle(context);
}

}  // namespace crostini
