// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that this file only tests the basic behavior of the cache counter, as in
// when it counts and when not, when result is nonzero and when not. It does not
// test whether the result of the counting is correct. This is the
// responsibility of a lower layer, and is tested in
// DiskCacheBackendTest.CalculateSizeOfAllEntries in net_unittests.

#include "chrome/browser/browsing_data/counters/cache_counter.h"

#include <memory>

#include "base/functional/bind.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
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
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        browser()
            ->profile()
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        simple_loader_helper.GetCallbackDeprecated());
    simple_loader_helper.WaitForCallback();
  }

  void WaitForCountingResult() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
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
IN_PROC_BROWSER_TEST_F(CacheCounterTest, Empty) {
  Profile* profile = browser()->profile();

  // Clear the |profile| to ensure that there was no data added from other
  // processes unrelated to this test.
  base::RunLoop wait_until_empty;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearHttpCache(base::Time(), base::Time::Max(), nullptr,
                       wait_until_empty.QuitClosure());
  wait_until_empty.Run();

  // This test occasionally flakes, where the cache size is still seen as
  // non-zero after deletion. However, the exact value is consistent across all
  // flakes observed within the same day, which indicates that there is a
  // deterministic process writing into cache but with indeterministic timing,
  // so as to cause this test to flake.  Wait until the value is 0 as opposed to
  // checking it immediately. If this never happens, the test will fail with a
  // timeout. Note that this only works if the process that populates the cache
  // runs before our deletion - in that case the delay ensures that the deletion
  // finishes. If this process happens after deletion, then this doesn't help
  // and the test will still fail.
  while (true) {
    CacheCounter counter(profile);
    counter.Init(profile->GetPrefs(),
                 browsing_data::ClearBrowsingDataTab::ADVANCED,
                 base::BindRepeating(&CacheCounterTest::CountingCallback,
                                     base::Unretained(this)));
    counter.Restart();
    WaitForCountingResult();

    if (GetResult() == 0u)
      break;
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }
  EXPECT_EQ(0u, GetResult());
}

// Tests that for a non-empty cache, the result is nonzero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, NonEmpty) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCountingResult();

  EXPECT_NE(0u, GetResult());
}

// Tests that after dooming a nonempty cache, the result is zero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, AfterDoom) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));

  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearHttpCache(
          base::Time(), base::Time::Max(), nullptr,
          base::BindOnce(&CacheCounter::Restart, base::Unretained(&counter)));

  WaitForCountingResult();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PrefChanged) {
  SetCacheDeletionPref(false);

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  SetCacheDeletionPref(true);

  // Test that changing the pref causes the counter to be restarted. If it
  // doesn't, this WaitForCountingResult() statement will time out. The actual
  // value returned by the counter is not important.
  WaitForCountingResult();
}

// Tests that the counting is restarted when the time period changes.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PeriodChanged) {
  CreateCacheEntry();

  Profile* profile = browser()->profile();
  CacheCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForCountingResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForCountingResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForCountingResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForCountingResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForCountingResult();
}

}  // namespace
