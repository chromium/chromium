// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"

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
  return base::Singleton<AccessibilityLabelsServiceFactory>::get();
}

// static
KeyedService* AccessibilityLabelsServiceFactory::BuildInstanceFor(
    Profile* profile) {
  return new AccessibilityLabelsService(profile);
}

AccessibilityLabelsServiceFactory::AccessibilityLabelsServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityLabelsService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AccessibilityLabelsServiceFactory::~AccessibilityLabelsServiceFactory() {}

KeyedService* AccessibilityLabelsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
