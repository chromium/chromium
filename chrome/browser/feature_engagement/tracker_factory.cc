// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/tracker_factory.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace feature_engagement {

// static
TrackerFactory* TrackerFactory::GetInstance() {
  return base::Singleton<TrackerFactory>::get();
}

// static
feature_engagement::Tracker* TrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<feature_engagement::Tracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

TrackerFactory::TrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "feature_engagement::Tracker",
          BrowserContextDependencyManager::GetInstance()) {
}

TrackerFactory::~TrackerFactory() = default;

KeyedService* TrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::BEST_EFFORT});

  base::FilePath storage_dir = profile->GetPath().Append(
      chrome::kFeatureEngagementTrackerStorageDirname);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();
  return feature_engagement::Tracker::Create(
      storage_dir, background_task_runner, db_provider);
}

content::BrowserContext* TrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace feature_engagement
