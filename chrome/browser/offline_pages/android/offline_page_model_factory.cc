// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_model_factory.h"

#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/offline_pages/android/offline_page_archive_publisher_impl.h"
#include "chrome/browser/offline_pages/download_archive_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"

namespace offline_pages {

OfflinePageModelFactory::OfflinePageModelFactory()
    : SimpleKeyedServiceFactory("OfflinePageModel",
                                SimpleDependencyManager::GetInstance()) {}

// static
OfflinePageModelFactory* OfflinePageModelFactory::GetInstance() {
  static base::NoDestructor<OfflinePageModelFactory> instance;
  return instance.get();
}

// static
OfflinePageModel* OfflinePageModelFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<OfflinePageModel*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

// static
OfflinePageModel* OfflinePageModelFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return GetForKey(profile->GetProfileKey());
}

std::unique_ptr<KeyedService> OfflinePageModelFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  base::FilePath store_path =
      key->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStore(background_task_runner, store_path));

  base::FilePath persistent_archives_dir =
      key->GetPath().Append(chrome::kOfflinePageArchivesDirname);
  // If base::PathService::Get returns false, the temporary_archives_dir will be
  // empty, and no temporary pages will be saved during this chrome lifecycle.
  base::FilePath temporary_archives_dir;
  if (base::PathService::Get(base::DIR_CACHE, &temporary_archives_dir)) {
    temporary_archives_dir =
        temporary_archives_dir.Append(chrome::kOfflinePageArchivesDirname);
  }

  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  auto archive_manager = std::make_unique<DownloadArchiveManager>(
      temporary_archives_dir, persistent_archives_dir,
      DownloadPrefs::GetDefaultDownloadDirectory(), background_task_runner,
      profile_key->GetPrefs());
  auto clock = std::make_unique<base::DefaultClock>();

  auto publisher =
      std::make_unique<OfflinePageArchivePublisherImpl>(archive_manager.get());

  std::unique_ptr<OfflinePageModelTaskified> model =
      std::make_unique<OfflinePageModelTaskified>(
          std::move(metadata_store), std::move(archive_manager),
          std::move(publisher), background_task_runner);

  return model;
}

}  // namespace offline_pages
