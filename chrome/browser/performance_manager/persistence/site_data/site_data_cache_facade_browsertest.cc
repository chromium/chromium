// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

std::ostream& operator<<(std::ostream& os, SiteFeatureUsage feature_usage) {
  switch (feature_usage) {
    case SiteFeatureUsage::kSiteFeatureUsageUnknown:
      return os << "UsageUnknown";
    case SiteFeatureUsage::kSiteFeatureInUse:
      return os << "InUse";
    case SiteFeatureUsage::kSiteFeatureNotInUse:
      return os << "NotInUse";
  }
  return os << "invalid SiteFeatureUsage " << static_cast<int>(feature_usage);
}

namespace {

static constexpr char kSiteA[] = "a.com";

// An observer that clears all site data for a profile at the moment it's
// destroyed. In tests that call CloseAllBrowsers() at the end of the test, the
// Profile for each browser is destroyed during the last cycle of the UI thread
// message loop that processes outstanding messages just before starting the
// browser shutdown. At this point any tasks posted to a non-BLOCK_SHUTDOWN
// sequence will be lost, so this is a good time to call ClearAllSiteData() to
// verify that it succeeds.
class ClearSiteDataOnProfileDestroyed final : public ProfileObserver {
 public:
  // Starts watching `profile`.
  explicit ClearSiteDataOnProfileDestroyed(Profile* profile) {
    profile_observation_.Observe(profile);
  }

  // Calls ClearAllSiteData() for `profile`. This is called when `profile` still
  // exists, but it's too late to prevent its destruction.
  void OnProfileWillBeDestroyed(Profile* profile) final {
    profile_observation_.Reset();
    auto* facade_factory = SiteDataCacheFacadeFactory::GetInstance();
    ASSERT_TRUE(facade_factory);
    auto* profile_facade = facade_factory->GetProfileFacadeForTesting(profile);
    ASSERT_TRUE(profile_facade);
    profile_facade->ClearAllSiteDataForTesting();
  }

 private:
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

// Overrides heuristics that are fragile to fake in a test.
class TestSiteDataRecorderHeuristics final : public SiteDataRecorderHeuristics {
 public:
  // Delegates to the production implementation of IsInBackground() because
  // that state can be controlled in the test.
  bool IsInBackground(const PageNode* page_node) const final {
    return DefaultIsInBackground(page_node);
  }

  bool IsLoadedIdle(PageNode::LoadingState) const final { return true; }

  bool IsOutsideLoadingGracePeriod(const PageNode*,
                                   FeatureType,
                                   base::TimeDelta) const final {
    return true;
  }

  bool IsOutsideBackgroundingGracePeriod(const PageNode*,
                                         FeatureType,
                                         base::TimeDelta) const final {
    return true;
  }
};

struct PMThreadingConfiguration {
  bool run_on_main_thread;
  bool run_on_main_thread_sync;
};

// Tests SiteDataCacheFacade in different threading configurations.
class SiteDataCacheFacadeBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
  using Super = InProcessBrowserTest;

 protected:
  SiteDataCacheFacadeBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kRunOnMainThreadSync,
                                              GetParam());
  }

  void SetUpOnMainThread() override {
    Super::SetUpOnMainThread();

    RunInGraph([] {
      SiteDataRecorder::SetHeuristicsImplementationForTesting(
          std::make_unique<TestSiteDataRecorderHeuristics>());
    });

    // Serve test HTML from any domain.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    RunInGraph([] {
      SiteDataRecorder::SetHeuristicsImplementationForTesting(nullptr);
    });
    Super::TearDownOnMainThread();
  }

  // Tests what SiteDataCache has recorded for the "updates_title_in_background"
  // feature of `site`. The feature usage should be
  // `expected_updates_title_in_background` and the dirty bit should be
  // `expected_is_dirty` (true if the feature usage is only in the in-memory
  // cache, false if it's been written to disk). If `site` isn't in the
  // SiteDataCache, the feature usage will be kSiteFeatureUsageUnknown and the
  // dirty bit will be false.
  void ExpectSiteData(const std::string& site,
                      SiteFeatureUsage expected_updates_title_in_background,
                      bool expected_is_dirty) {
    const std::string browser_context_id = browser()->profile()->UniqueId();
    const url::Origin origin = embedded_test_server()->GetOrigin(site);

    // Look up the reader and writer for `origin`.
    std::unique_ptr<SiteDataReader> reader;
    std::unique_ptr<SiteDataWriter> writer;
    RunInGraph([&](base::OnceClosure quit_closure) {
      // Remember to quit the RunLoop on early return.
      base::ScopedClosureRunner quit_on_exit(std::move(quit_closure));

      auto* factory = SiteDataCacheFactory::GetInstance();
      ASSERT_TRUE(factory);
      SiteDataCacheInspector* inspector =
          factory->GetInspectorForBrowserContext(browser_context_id);
      ASSERT_TRUE(inspector);
      SiteDataCache* cache = inspector->GetDataCache();
      ASSERT_TRUE(cache);
      reader = cache->GetReaderForOrigin(origin);
      ASSERT_TRUE(reader);
      writer = cache->GetWriterForOrigin(origin);
      ASSERT_TRUE(writer);

      // Wait until the reader finishes asynchronously loading data.
      // The reader and writer can't be destroyed inside the callback, so exit
      // the runloop and start a new one when it fires.
      reader->RegisterDataLoadedCallback(quit_on_exit.Release());
    });

    RunInGraph([&] {
      EXPECT_EQ(reader->UpdatesTitleInBackground(),
                expected_updates_title_in_background);
      EXPECT_EQ(writer->impl_for_testing()->is_dirty(), expected_is_dirty);

      // The reader and writer must be destroyed on the graph sequence.
      reader.reset();
      writer.reset();
    });
  }

  // Adapted from InProcessBrowserTest::AddTabAtIndex() to open a background
  // tab. Waits for the new tab to load, and returns its WebContents, or
  // nullptr if the load failed.
  content::WebContents* AddBackgroundTabAtIndex(int index, const GURL& url) {
    NavigateParams params(browser(), url,
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    params.tabstrip_index = index;
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    Navigate(&params);
    content::WebContents* web_contents = params.navigated_or_inserted_contents;
    return content::WaitForLoadStop(web_contents) ? web_contents : nullptr;
  }

  // Adds a tab that loads a page at `site` and updates its title to `title` in
  // the background. This should be recorded by the SiteDataRecorder.
  void AddTabAndUpdateTitleInBackground(std::string_view site,
                                        std::string_view title) {
    // Start from title2.html, which already has a title, because
    // PerformanceManagerTabHelper::TitleWasSet() ignores the first update to
    // a page that has no title on load.
    content::WebContents* web_contents = AddBackgroundTabAtIndex(
        1, embedded_test_server()->GetURL(site, "/title2.html"));
    ASSERT_TRUE(web_contents);
    EXPECT_EQ(web_contents->GetVisibility(), content::Visibility::HIDDEN);

    const std::u16string utf16_title = base::ASCIIToUTF16(title);
    content::TitleWatcher title_watcher(web_contents, utf16_title);
    ASSERT_TRUE(content::ExecJs(
        web_contents, base::StrCat({"document.title = '", title, "'"})));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), utf16_title);
  }

  // Starts watching for shutdown and triggers ClearAllSiteData() at the last
  // possible time.
  void ClearAllSiteDataOnShutdown() {
    clear_site_data_.emplace(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ClearSiteDataOnProfileDestroyed> clear_site_data_;
};

INSTANTIATE_TEST_SUITE_P(, SiteDataCacheFacadeBrowserTest, ::testing::Bool());

// TODO(crbug.com/330771327): This test is consistently failing across multiple
// builders. Pre-test: Sets up state before the main test by writing some
// feature usage for a site.
IN_PROC_BROWSER_TEST_P(SiteDataCacheFacadeBrowserTest,
                       DISABLED_PRE_PRE_ClearAllSiteData) {
  // Should start from a clean profile.
  ExpectSiteData(kSiteA, SiteFeatureUsage::kSiteFeatureUsageUnknown,
                 /*is_dirty=*/false);
  AddTabAndUpdateTitleInBackground(kSiteA, "New Title");

  // Check that SiteDataRecorder observed the feature usage and will write
  // it to the DB.
  ExpectSiteData(kSiteA, SiteFeatureUsage::kSiteFeatureInUse,
                 /*is_dirty=*/true);
  CloseAllBrowsers();
}

// TODO(crbug.com/330771327): This test is consistently failing across multiple
// builders. Main test: clears the feature usage written in
// PRE_PRE_ClearAllSiteData, to validate that the DB is updated when racing with
// shutdown.
IN_PROC_BROWSER_TEST_P(SiteDataCacheFacadeBrowserTest,
                       DISABLED_PRE_ClearAllSiteData) {
  // Make sure the site DB was written before the browser restarted.
  ExpectSiteData(kSiteA, SiteFeatureUsage::kSiteFeatureInUse,
                 /*is_dirty=*/false);
  ClearAllSiteDataOnShutdown();
  CloseAllBrowsers();
}

// TODO(crbug.com/330771327): This test is consistently failing across multiple
// builders. Post-test: validates that PRE_ClearAllSiteData deleted the feature
// usage written in PRE_PRE_ClearAllSiteData.
IN_PROC_BROWSER_TEST_P(SiteDataCacheFacadeBrowserTest,
                       DISABLED_ClearAllSiteData) {
  // Site data should have been deleted before browser exited.
  ExpectSiteData(kSiteA, SiteFeatureUsage::kSiteFeatureUsageUnknown,
                 /*is_dirty=*/false);
  CloseAllBrowsers();
}

}  // namespace

}  // namespace performance_manager
