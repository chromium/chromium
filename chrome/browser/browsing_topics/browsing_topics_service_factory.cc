// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

// static
BrowsingTopicsService* BrowsingTopicsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BrowsingTopicsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
BrowsingTopicsServiceFactory* BrowsingTopicsServiceFactory::GetInstance() {
  static base::NoDestructor<BrowsingTopicsServiceFactory> factory;
  return factory.get();
}

BrowsingTopicsServiceFactory::BrowsingTopicsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingTopicsService",
          BrowserContextDependencyManager::GetInstance()) {}

BrowsingTopicsServiceFactory::~BrowsingTopicsServiceFactory() = default;

KeyedService* BrowsingTopicsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(blink::features::kBrowsingTopics))
    return nullptr;

  return new BrowsingTopicsServiceImpl();
}

bool BrowsingTopicsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The `BrowsingTopicsService` needs to be created with Profile, as it needs
  // to schedule the topics calculation right away, and it might also need to
  // handle some data deletion on startup.
  return true;
}

}  // namespace browsing_topics
