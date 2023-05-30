// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/profiles/profile.h"

// static
AccessibilityLabelsService* AccessibilityLabelsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccessibilityLabelsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityLabelsService*
AccessibilityLabelsServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<AccessibilityLabelsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
AccessibilityLabelsServiceFactory*
AccessibilityLabelsServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityLabelsServiceFactory> instance;
  return instance.get();
}

AccessibilityLabelsServiceFactory::AccessibilityLabelsServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityLabelsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not display web content and thus do not need the
              // accessibility labels service.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

AccessibilityLabelsServiceFactory::~AccessibilityLabelsServiceFactory() =
    default;

KeyedService* AccessibilityLabelsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccessibilityLabelsService(Profile::FromBrowserContext(context));
}
