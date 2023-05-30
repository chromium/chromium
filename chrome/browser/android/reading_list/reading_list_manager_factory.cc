// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/reading_list/reading_list_manager_factory.h"

#include <memory>

#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "components/reading_list/features/reading_list_switches.h"

// static
ReadingListManagerFactory* ReadingListManagerFactory::GetInstance() {
  static base::NoDestructor<ReadingListManagerFactory> instance;
  return instance.get();
}

// static
ReadingListManager* ReadingListManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

ReadingListManagerFactory::ReadingListManagerFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ReadingListModelFactory::GetInstance());
}

ReadingListManagerFactory::~ReadingListManagerFactory() = default;

KeyedService* ReadingListManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(context);
  return new ReadingListManagerImpl(reading_list_model);
}
