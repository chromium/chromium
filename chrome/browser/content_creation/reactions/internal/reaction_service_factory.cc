// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_creation/reactions/internal/reaction_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_creation/reactions/core/reaction_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace content_creation {

// static
ReactionServiceFactory* ReactionServiceFactory::GetInstance() {
  return base::Singleton<ReactionServiceFactory>::get();
}

// static
ReactionService* ReactionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ReactionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ReactionServiceFactory::ReactionServiceFactory()
    : ProfileKeyedServiceFactory("ReactionService") {}

ReactionServiceFactory::~ReactionServiceFactory() = default;

KeyedService* ReactionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ReactionService();
}

}  // namespace content_creation
