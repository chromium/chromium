// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_database.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {
namespace testing {

// Return the LocalSiteCharacteristicsDataImpl instance backing a WebContents,
// this might be null if this WebContents isn't loaded with a valid URL.
internal::LocalSiteCharacteristicsDataImpl*
GetLocalSiteCharacteristicsDataImplForWC(content::WebContents* web_contents);

// Wait for the Local Site Characteristics Database entry for a given
// WebContents to be initialized, run |run_pending_tasks| repeatedly while
// waiting.
void WaitForLocalDBEntryToBeInitialized(
    content::WebContents* web_contents,
    base::RepeatingClosure run_pending_tasks);

// Expire all the Local Site Characteristics Database observation windows
// for a given WebContents.
void ExpireLocalDBObservationWindows(content::WebContents* web_contents);

// Pretend that this WebContents has been loaded in background.
void MarkWebContentsAsLoadedInBackground(content::WebContents* web_contents);

class MockLocalSiteCharacteristicsDataImplOnDestroyDelegate
    : public internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate {
 public:
  MockLocalSiteCharacteristicsDataImplOnDestroyDelegate();
  ~MockLocalSiteCharacteristicsDataImplOnDestroyDelegate();

  MOCK_METHOD1(OnLocalSiteCharacteristicsDataImplDestroyed,
               void(internal::LocalSiteCharacteristicsDataImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MockLocalSiteCharacteristicsDataImplOnDestroyDelegate);
};

// An implementation of a LocalSiteCharacteristicsDatabase that doesn't record
// anything.
class NoopLocalSiteCharacteristicsDatabase
    : public LocalSiteCharacteristicsDatabase {
 public:
  NoopLocalSiteCharacteristicsDatabase();
  ~NoopLocalSiteCharacteristicsDatabase() override;

  // LocalSiteCharacteristicsDatabase:
  void ReadSiteCharacteristicsFromDB(
      const url::Origin& origin,
      ReadSiteCharacteristicsFromDBCallback callback) override;
  void WriteSiteCharacteristicsIntoDB(
      const url::Origin& origin,
      const SiteCharacteristicsProto& site_characteristic_proto) override;
  void RemoveSiteCharacteristicsFromDB(
      const std::vector<url::Origin>& site_origins) override;
  void ClearDatabase() override;
  void GetDatabaseSize(GetDatabaseSizeCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopLocalSiteCharacteristicsDatabase);
};

// A wrapper around ChromeRenderViewHostTestHarness that ensures that the Local
// Site Characteristics Database is initialized.
class ChromeTestHarnessWithLocalDB : public ChromeRenderViewHostTestHarness {
 public:
  ChromeTestHarnessWithLocalDB();
  ~ChromeTestHarnessWithLocalDB() override;

  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace testing
}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_
