// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that this file only tests the basic behavior of the cache counter, as in
// when it counts and when not, when result is nonzero and when not. It does not
// test whether the result of the counting is correct. This is the
// responsibility of a lower layer, and is tested in
// DiskCacheBackendTest.CalculateSizeOfAllEntries in net_unittests.

#include "chrome/browser/browsing_data/counters/cache_counter.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

class CacheCounterTest : public InProcessBrowserTest {
 public:
  CacheCounterTest() {}

  void SetUpOnMainThread() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    SetCacheDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void SetCacheDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteCache, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void CreateCacheEntry() {
    // A cache entry is synthesized by fetching a cacheable URL
    // from the test server.
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = embedded_test_server()->GetURL("/cachetime/yay");

    // Populate the Network Isolation Key so that it is cacheable.
    url::Origin origin =
        url::Origin::Create(embedded_test_server()->base_url());
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->network_isolation_key =
        net::NetworkIsolationKey(origin, origin);

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile())
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
  }

  void WaitForCountingResult() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  // Callback from the counter.
  void CountingCallback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    finished_ = result->Finished();

    if (finished_) {
      auto* cache_result =
          static_cast<CacheCounter::CacheResult*>(result.get());
      result_ = cache_result->cache_size();
      is_upper_limit_ = cache_result->is_upper_limit();
    }

    if (run_loop_ && finished_)
      run_loop_->Quit();
  }

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  bool IsUpperLimit() {
    DCHECK(finished_);
    return is_upper_limit_;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
  bool is_upper_limit_;
};

// Tests that for the empty cache, the result is zero.
// Flaky. See crbug.com/971650.
#if defined(OS_LINUX)
#define MAYBE_Empty DISABLED_Empty
#else
#define MAYBE_Empty Empty
#endif
IN_PROC_BROWSER_TEST_F(CacheCounterTest, MAYBE_Empty) {
  Profile* profile = browser()->profile();

  CacheCounter counter(profile);
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&CacheCounterTest::CountingCallback, base::Unretained(this)));
  counter.Restart();

  WaitForCountingResult();
  EXPECT_EQ(0u, GetResult());
}

// Tests that for a non-empty cache, the result is nonzero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, NonEmpty) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&CacheCounterTest::CountingCallback, base::Unretained(this)));
  counter.Restart();

  WaitForCountingResult();

  EXPECT_NE(0u, GetResult());
}

// Tests that after dooming a nonempty cache, the result is zero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, AfterDoom) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&CacheCounterTest::CountingCallback, base::Unretained(this)));

  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->GetNetworkContext()
      ->ClearHttpCache(
          base::Time(), base::Time::Max(), nullptr,
          base::BindOnce(&CacheCounter::Restart, base::Unretained(&counter)));

  WaitForCountingResult();
  EXPECT_EQ(0u, GetResult());
}

// TODO(crbug.com/985131): Test is flaky in Linux, Win and ChromeOS.
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_PrefChanged DISABLED_PrefChanged
#else
#define MAYBE_PrefChanged PrefChanged
#endif

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, MAYBE_PrefChanged) {
  SetCacheDeletionPref(false);

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&CacheCounterTest::CountingCallback, base::Unretained(this)));
  SetCacheDeletionPref(true);

  WaitForCountingResult();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counting is restarted when the time period changes.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PeriodChanged) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&CacheCounterTest::CountingCallback, base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForCountingResult();
  browsing_data::BrowsingDataCounter::ResultInt result = GetResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForCountingResult();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForCountingResult();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForCountingResult();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForCountingResult();
  EXPECT_EQ(result, GetResult());
  EXPECT_FALSE(IsUpperLimit());
}

}  // namespace
