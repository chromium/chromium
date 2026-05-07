// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/accessibility_annotator_first_run_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_enablement_service_factory.h"
#include "chrome/browser/accessibility_annotator/first_run/chrome_accessibility_annotator_first_run_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

// static
accessibility_annotator::AccessibilityAnnotatorFirstRunService*
AccessibilityAnnotatorFirstRunServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<
      accessibility_annotator::AccessibilityAnnotatorFirstRunService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AccessibilityAnnotatorFirstRunServiceFactory*
AccessibilityAnnotatorFirstRunServiceFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorFirstRunServiceFactory>
      instance;
  return instance.get();
}

AccessibilityAnnotatorFirstRunServiceFactory::
    AccessibilityAnnotatorFirstRunServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorFirstRunService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AccessibilityAnnotatorEnablementServiceFactory::GetInstance());
}

AccessibilityAnnotatorFirstRunServiceFactory::
    ~AccessibilityAnnotatorFirstRunServiceFactory() = default;

std::unique_ptr<KeyedService> AccessibilityAnnotatorFirstRunServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!accessibility_annotator::features::
          IsAccessibilityAnnotatorFirstRunEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<accessibility_annotator::AccessibilityAnnotatorFirstRunClient>
      client = std::make_unique<ChromeAccessibilityAnnotatorFirstRunClient>();
  return std::make_unique<
      accessibility_annotator::AccessibilityAnnotatorFirstRunServiceImpl>(
      std::move(client),
      AccessibilityAnnotatorEnablementServiceFactory::GetForProfile(profile),
      profile->GetPrefs());
}
