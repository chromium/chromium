// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_SITE_DATA_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_SITE_DATA_UTILS_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/functional/callback_helpers.h"
#include "components/performance_manager/test_support/test_harness_helper.h"

class Profile;

namespace content {
class WebContents;
}

namespace performance_manager {

namespace internal {
class SiteDataImpl;
}

class PageNode;

// A test harness that initializes all the components required to use the
// SiteData database in unit_tests. This doesn't support tests that use multiple
// profiles. See comment in the PerformanceManagerTestHarnessHelper class for
// directions on how this should be used in tests.
class SiteDataTestHarness : public PerformanceManagerTestHarnessHelper {
 public:
  SiteDataTestHarness();
  SiteDataTestHarness(const SiteDataTestHarness& other) = delete;
  SiteDataTestHarness& operator=(const SiteDataTestHarness&) = delete;
  ~SiteDataTestHarness() override;

  void SetUp() override;
  // During tear down it's necessary to release the global
  // SiteDataCacheFacadeFactory instance to ensure that it doesn't get reused
  // from tests to tests. Before doing this it's necessary to disassociate it
  // from any profile. This harness assumes that tests are only using one
  // profile.
  void TearDown(Profile* profile);

 private:
  void TearDown() override;

  // Use an in memory database to avoid creating some unnecessary files on disk.
  base::ScopedClosureRunner use_in_memory_db_for_testing_;
  std::unique_ptr<base::AutoReset<bool>> enable_cache_factory_for_testing_;
};

// Return the SiteDataImpl instance backing a PageNode, this might be null if
// this PageNode isn't loaded with a valid URL.
//
// This function can only be called from the graph's sequence.
internal::SiteDataImpl* GetSiteDataImplForPageNode(PageNode* page_node);

// Pretend that this WebContents has been loaded and is in background.
void MarkWebContentsAsLoadedInBackgroundInSiteDataDb(
    content::WebContents* web_contents);

// Pretend that this WebContents has been unloaded and is in background.
void MarkWebContentsAsUnloadedInBackgroundInSiteDataDb(
    content::WebContents* web_contents);

// Expire all the Site Data Database observation windows for a given
// WebContents.
void ExpireSiteDataObservationWindowsForWebContents(
    content::WebContents* web_contents);

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_SITE_DATA_UTILS_H_
