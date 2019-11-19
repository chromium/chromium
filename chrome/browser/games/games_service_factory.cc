// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/games/games_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/games/core/games_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace games {

GamesServiceFactory::GamesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "GamesService",
          BrowserContextDependencyManager::GetInstance()) {}

GamesServiceFactory::~GamesServiceFactory() = default;

// static
GamesServiceFactory* GamesServiceFactory::GetInstance() {
  return base::Singleton<GamesServiceFactory>::get();
}

// static
GamesService* GamesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<GamesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* GamesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GamesServiceImpl(profile->GetPrefs());
}

}  // namespace games
