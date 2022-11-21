// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_test_archive_publisher.h"

namespace {
const int64_t kDownloadId = 42LL;
}  // namespace

namespace offline_pages {

std::unique_ptr<KeyedService> BuildTestOfflinePageModel(SimpleFactoryKey* key) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  base::FilePath store_path =
      key->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStore(task_runner, store_path));

  base::FilePath private_archives_dir =
      key->GetPath().Append(chrome::kOfflinePageArchivesDirname);
  base::FilePath public_archives_dir("/sdcard/Download");
  // If base::PathService::Get returns false, the temporary_archives_dir will be
  // empty, and no temporary pages will be saved during this chrome lifecycle.
  base::FilePath temporary_archives_dir;
  if (base::PathService::Get(base::DIR_CACHE, &temporary_archives_dir)) {
    temporary_archives_dir =
        temporary_archives_dir.Append(chrome::kOfflinePageArchivesDirname);
  }
  auto archive_manager = std::make_unique<ArchiveManager>(
      temporary_archives_dir, private_archives_dir, public_archives_dir,
      task_runner);
  auto publisher = std::make_unique<OfflinePageTestArchivePublisher>(
      archive_manager.get(), kDownloadId);

  return std::unique_ptr<KeyedService>(new OfflinePageModelTaskified(
      std::move(metadata_store), std::move(archive_manager),
      std::move(publisher), task_runner));
}

}  // namespace offline_pages
