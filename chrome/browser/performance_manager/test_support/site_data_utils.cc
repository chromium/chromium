// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/site_data_utils.h"
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

SiteDataTestHarness::SiteDataTestHarness()
    : use_in_memory_db_for_testing_(
          LevelDBSiteDataStore::UseInMemoryDBForTesting()),
      enable_cache_factory_for_testing_(
          SiteDataCacheFacadeFactory::EnableForTesting()) {}

SiteDataTestHarness::~SiteDataTestHarness() = default;

void SiteDataTestHarness::SetUp() {
  PerformanceManagerTestHarnessHelper::SetUp();
  PerformanceManager::GetGraph()->PassToGraph(
      std::make_unique<SiteDataRecorder>());
}

void SiteDataTestHarness::TearDown(Profile* profile) {
  SiteDataCacheFacadeFactory::DisassociateForTesting(profile);
  TearDown();
}

void SiteDataTestHarness::TearDown() {
  PerformanceManagerTestHarnessHelper::TearDown();
}

internal::SiteDataImpl* GetSiteDataImplForPageNode(PageNode* page_node) {
  auto* writer = SiteDataRecorder::Data::FromPageNode(page_node).writer();

  if (!writer)
    return nullptr;

  return writer->impl_for_testing();
}

void MarkWebContentsAsLoadedInBackgroundInSiteDataDb(
    content::WebContents* web_contents) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);
  auto* impl = GetSiteDataImplForPageNode(page_node.get());
  CHECK(impl);
  impl->NotifySiteLoaded();
  impl->NotifyLoadedSiteBackgrounded();
}

void MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
    content::WebContents* web_contents) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);
  auto* impl = GetSiteDataImplForPageNode(page_node.get());
  CHECK(impl);
  impl->NotifySiteUnloaded(TabVisibility::kBackground);
}

void ExpireSiteDataObservationWindowsForWebContents(
    content::WebContents* web_contents) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents);
  CHECK(page_node);
  auto* impl = GetSiteDataImplForPageNode(page_node.get());
  CHECK(impl);
  impl->ExpireAllObservationWindowsForTesting();
}

}  // namespace performance_manager
