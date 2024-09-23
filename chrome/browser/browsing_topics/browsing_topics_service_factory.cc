// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_topics/annotator.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/browsing_topics/annotator_impl.h"
#else
#include "components/browsing_topics/annotator_noop.h"
#endif

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
    : ProfileKeyedServiceFactory(
          "BrowsingTopicsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
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

  OptimizationGuideKeyedService* opt_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!opt_guide_service) {
    return nullptr;
  }

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata");
  optimization_guide::proto::PageTopicsModelMetadata model_metadata;
  // The current version the client supports for the topics model. This
  // should be incremented any time there is a client code change to how the
  // topics model works that needs to be side-channeled to the server.
  model_metadata.set_version(2);
  model_metadata.set_taxonomy_version(
      blink::features::kBrowsingTopicsTaxonomyVersion.Get());
  model_metadata.SerializeToString(any_metadata.mutable_value());

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<Annotator> annotator = std::make_unique<AnnotatorImpl>(
      /*model_provider=*/opt_guide_service,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      any_metadata);
#else
  std::unique_ptr<Annotator> annotator = std::make_unique<AnnotatorNoOp>();
#endif

  return new BrowsingTopicsServiceImpl(
      profile->GetPath(), privacy_sandbox_settings, history_service,
      site_data_manager, std::move(annotator),
      base::BindRepeating(
          content_settings::PageSpecificContentSettings::TopicAccessed));
}

bool BrowsingTopicsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The `BrowsingTopicsService` needs to be created with Profile, as it needs
  // to schedule the topics calculation right away, and it might also need to
  // handle some data deletion on startup.
  return true;
}

}  // namespace browsing_topics
