// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/get_smart_card_context_factory.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker_impl.h"

// static
SmartCardReaderTrackerFactory* SmartCardReaderTrackerFactory::GetInstance() {
  static base::NoDestructor<SmartCardReaderTrackerFactory> factory;
  return factory.get();
}

// static
SmartCardReaderTracker& SmartCardReaderTrackerFactory::GetForProfile(
    Profile& profile) {
  return CHECK_DEREF(static_cast<SmartCardReaderTracker*>(
      GetInstance()->GetServiceForBrowserContext(&profile, /*create=*/true)));
}

SmartCardReaderTrackerFactory::SmartCardReaderTrackerFactory()
    : ProfileKeyedServiceFactory(
          "SmartCardReaderTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

SmartCardReaderTrackerFactory::~SmartCardReaderTrackerFactory() = default;

std::unique_ptr<KeyedService>
SmartCardReaderTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SmartCardReaderTrackerImpl>(
      GetSmartCardContextFactory(*context));
}
