// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/login_detection/login_detection_keyed_service.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace login_detection {
namespace {

ProfileSelections BuildLoginDetectionProfileSelection() {
  if (!IsLoginDetectionFeatureEnabled()) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      // TODO(crbug.com/40257657): Check if this service is needed in Guest
      // mode.
      .WithGuest(ProfileSelection::kOriginalOnly)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kOriginalOnly)
      .Build();
}

}  // namespace

// static
LoginDetectionKeyedService* LoginDetectionKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LoginDetectionKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LoginDetectionKeyedServiceFactory*
LoginDetectionKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<LoginDetectionKeyedServiceFactory> factory;
  return factory.get();
}

LoginDetectionKeyedServiceFactory::LoginDetectionKeyedServiceFactory()
    : ProfileKeyedServiceFactory("LoginDetectionKeyedService",
                                 BuildLoginDetectionProfileSelection()) {}

LoginDetectionKeyedServiceFactory::~LoginDetectionKeyedServiceFactory() =
    default;

KeyedService* LoginDetectionKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LoginDetectionKeyedService(Profile::FromBrowserContext(context));
}

bool LoginDetectionKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Required, since the service's constructor applies site isolation for saved
  // login sites, which needs to happen at profile initialization time, before
  // any navigations happen in it.
  return true;
}

}  // namespace login_detection
