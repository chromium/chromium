// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace resource_coordinator {
namespace testing {

internal::LocalSiteCharacteristicsDataImpl*
GetLocalSiteCharacteristicsDataImplForWC(content::WebContents* web_contents) {
  ResourceCoordinatorTabHelper* tab_helper =
      ResourceCoordinatorTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  auto* wc_observer =
      tab_helper->local_site_characteristics_wc_observer_for_testing();
  DCHECK(wc_observer);

  auto* writer = static_cast<LocalSiteCharacteristicsDataWriter*>(
      wc_observer->GetWriterForTesting());

  if (!writer)
    return nullptr;

  return writer->impl_for_testing();
}

void WaitForLocalDBEntryToBeInitialized(
    content::WebContents* web_contents,
    base::RepeatingClosure run_pending_tasks) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetLocalSiteCharacteristicsDataImplForWC(web_contents);
  DCHECK(impl);
  while (!impl->fully_initialized_for_testing())
    run_pending_tasks.Run();
}

void ExpireLocalDBObservationWindows(content::WebContents* web_contents) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetLocalSiteCharacteristicsDataImplForWC(web_contents);
  DCHECK(impl);
  impl->ExpireAllObservationWindowsForTesting();
}

void MarkWebContentsAsLoadedInBackground(content::WebContents* web_contents) {
  internal::LocalSiteCharacteristicsDataImpl* impl =
      GetLocalSiteCharacteristicsDataImplForWC(web_contents);
  DCHECK(impl);
  impl->NotifySiteLoaded();
  impl->NotifyLoadedSiteBackgrounded();
}

MockLocalSiteCharacteristicsDataImplOnDestroyDelegate::
    MockLocalSiteCharacteristicsDataImplOnDestroyDelegate() = default;
MockLocalSiteCharacteristicsDataImplOnDestroyDelegate::
    ~MockLocalSiteCharacteristicsDataImplOnDestroyDelegate() = default;

NoopLocalSiteCharacteristicsDatabase::NoopLocalSiteCharacteristicsDatabase() =
    default;
NoopLocalSiteCharacteristicsDatabase::~NoopLocalSiteCharacteristicsDatabase() =
    default;

void NoopLocalSiteCharacteristicsDatabase::ReadSiteCharacteristicsFromDB(
    const url::Origin& origin,
    ReadSiteCharacteristicsFromDBCallback callback) {
  std::move(callback).Run(base::nullopt);
}

void NoopLocalSiteCharacteristicsDatabase::WriteSiteCharacteristicsIntoDB(
    const url::Origin& origin,
    const SiteCharacteristicsProto& site_characteristic_proto) {}

void NoopLocalSiteCharacteristicsDatabase::RemoveSiteCharacteristicsFromDB(
    const std::vector<url::Origin>& site_origins) {}

void NoopLocalSiteCharacteristicsDatabase::ClearDatabase() {}

void NoopLocalSiteCharacteristicsDatabase::GetDatabaseSize(
    GetDatabaseSizeCallback callback) {
  std::move(callback).Run(base::nullopt, base::nullopt);
}

ChromeTestHarnessWithLocalDB::ChromeTestHarnessWithLocalDB() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kSiteCharacteristicsDatabase);

  LocalSiteCharacteristicsWebContentsObserver::
      SkipObserverRegistrationForTesting();
}

ChromeTestHarnessWithLocalDB::~ChromeTestHarnessWithLocalDB() = default;

void ChromeTestHarnessWithLocalDB::SetUp() {
  // Enable the LocalSiteCharacteristicsDataStoreFactory before calling
  // ChromeRenderViewHostTestHarness::SetUp(), this will prevent the creation
  // of a non-mock version of a data store when browser_context() gets
  // initialized.
  LocalSiteCharacteristicsDataStoreFactory::EnableForTesting();

  ChromeRenderViewHostTestHarness::SetUp();
}

}  // namespace testing
}  // namespace resource_coordinator
