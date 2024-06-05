// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_main_node_annotator_controller_factory.h"

#include "chrome/browser/accessibility/ax_main_node_annotator_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace screen_ai {

// static
AXMainNodeAnnotatorController*
AXMainNodeAnnotatorControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<AXMainNodeAnnotatorController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AXMainNodeAnnotatorController*
AXMainNodeAnnotatorControllerFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<AXMainNodeAnnotatorController*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
AXMainNodeAnnotatorControllerFactory*
AXMainNodeAnnotatorControllerFactory::GetInstance() {
  static base::NoDestructor<AXMainNodeAnnotatorControllerFactory> instance;
  return instance.get();
}

AXMainNodeAnnotatorControllerFactory::AXMainNodeAnnotatorControllerFactory()
    : ProfileKeyedServiceFactory(
          "AXMainNodeAnnotatorController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not need the Main Node Annotator controller.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

AXMainNodeAnnotatorControllerFactory::~AXMainNodeAnnotatorControllerFactory() =
    default;

bool AXMainNodeAnnotatorControllerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
AXMainNodeAnnotatorControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AXMainNodeAnnotatorController>(
      Profile::FromBrowserContext(context));
}

}  // namespace screen_ai
