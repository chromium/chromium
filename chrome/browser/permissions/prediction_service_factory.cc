// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service_factory.h"

#include "base/memory/singleton.h"
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
  return base::Singleton<PredictionServiceFactory>::get();
}

PredictionServiceFactory::PredictionServiceFactory()
    : ProfileKeyedServiceFactory(
          "PredictionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

PredictionServiceFactory::~PredictionServiceFactory() = default;

KeyedService* PredictionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->shared_url_loader_factory());
  return new permissions::PredictionService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)));
}
