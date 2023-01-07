// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/storage_partition.h"
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
    : ProfileKeyedServiceFactory("BrowsingTopicsService") {
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
}

BrowsingTopicsServiceFactory::~BrowsingTopicsServiceFactory() = default;

KeyedService* BrowsingTopicsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(blink::features::kBrowsingTopics))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);

  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (!privacy_sandbox_settings)
    return nullptr;

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (!history_service)
    return nullptr;

  content::BrowsingTopicsSiteDataManager* site_data_manager =
      context->GetDefaultStoragePartition()->GetBrowsingTopicsSiteDataManager();
  if (!site_data_manager)
    return nullptr;

  optimization_guide::PageContentAnnotationsService* annotations_service =
      PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (!annotations_service)
    return nullptr;

  return new BrowsingTopicsServiceImpl(
      profile->GetPath(), privacy_sandbox_settings, history_service,
      site_data_manager, annotations_service);
}

bool BrowsingTopicsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The `BrowsingTopicsService` needs to be created with Profile, as it needs
  // to schedule the topics calculation right away, and it might also need to
  // handle some data deletion on startup.
  return true;
}

}  // namespace browsing_topics
