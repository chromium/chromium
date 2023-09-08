// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/read_write_cards/read_write_cards_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/read_write_cards/read_write_cards_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

constexpr char kReadWriteCardsServiceName[] = "ReadWriteCardsKeyedService";

}  // namespace

ReadWriteCardsFactory::ReadWriteCardsFactory()
    : ProfileKeyedServiceFactory(
          kReadWriteCardsServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

ReadWriteCardsFactory::~ReadWriteCardsFactory() = default;

ReadWriteCardsFactory* ReadWriteCardsFactory::GetInstance() {
  static base::NoDestructor<ReadWriteCardsFactory> instance;
  return instance.get();
}

ReadWriteCardsManager* ReadWriteCardsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ReadWriteCardsManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

std::unique_ptr<KeyedService>
ReadWriteCardsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<ReadWriteCardsManager>();
}

}  //  namespace chromeos
