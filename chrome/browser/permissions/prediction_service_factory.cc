// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/prediction_service/prediction_service.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

// static
permissions::PredictionService* PredictionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<permissions::PredictionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PredictionServiceFactory* PredictionServiceFactory::GetInstance() {
  static base::NoDestructor<PredictionServiceFactory> instance;
  return instance.get();
}

PredictionServiceFactory::PredictionServiceFactory()
    : ProfileKeyedServiceFactory(
          "PredictionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

PredictionServiceFactory::~PredictionServiceFactory() = default;

std::unique_ptr<KeyedService>
PredictionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->shared_url_loader_factory());
  return std::make_unique<permissions::PredictionService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)));
}
