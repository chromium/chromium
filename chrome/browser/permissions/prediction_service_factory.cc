// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "PredictionService",
          BrowserContextDependencyManager::GetInstance()) {}

PredictionServiceFactory::~PredictionServiceFactory() = default;

KeyedService* PredictionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->shared_url_loader_factory());
  return new permissions::PredictionService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)));
}

content::BrowserContext* PredictionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
