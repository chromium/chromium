// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

using browsing_data::ConditionalCacheCountingHelper;
using content::BrowserThread;

class ConditionalCacheCountingHelperBrowserTest : public InProcessBrowserTest {
 public:
  const int64_t kTimeoutMs = 1000;

  ConditionalCacheCountingHelperBrowserTest() {
    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void CountCallback(bool is_upper_limit, int64_t size) {
    // Negative values represent an unexpected error.
    DCHECK(size >= 0 || size == net::ERR_ABORTED);
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    last_size_ = size;
    last_is_upper_limit_ = is_upper_limit;

    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForTasksOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void CountEntries(base::Time begin_time, base::Time end_time) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    last_size_ = -1;
    ConditionalCacheCountingHelper::Count(
        browser()->profile()->GetDefaultStoragePartition(), begin_time,
        end_time,
        base::BindOnce(
            &ConditionalCacheCountingHelperBrowserTest::CountCallback,
            base::Unretained(this)));
  }

  int64_t GetResult() {
    DCHECK_GT(last_size_, 0);
    return last_size_;
  }

  int64_t IsUpperLimit() { return last_is_upper_limit_; }

  int64_t GetResultOrError() { return last_size_; }

  void CreateCacheEntries(const std::set<std::string>& keys) {
    for (const std::string& key : keys) {
      // A cache entry is synthesized by fetching a cacheable URL
      // from the test server.
      std::unique_ptr<network::ResourceRequest> request =
          std::make_unique<network::ResourceRequest>();
      request->url =
          embedded_test_server()->GetURL(base::StrCat({"/cachetime/", key}));

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
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  int64_t last_size_;
  bool last_is_upper_limit_;
};

// Tests that ConditionalCacheCountingHelper only counts those cache entries
// that match the condition.
// TODO(crbug.com/40816226): The test is flaky on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Count DISABLED_Count
#else
#define MAYBE_Count Count
#endif
IN_PROC_BROWSER_TEST_F(ConditionalCacheCountingHelperBrowserTest, MAYBE_Count) {
  // Create 5 entries.
  std::set<std::string> keys1 = {"1", "2", "3", "4", "5"};

  base::Time t1 = base::Time::Now();
  CreateCacheEntries(keys1);

  base::PlatformThread::Sleep(base::Milliseconds(kTimeoutMs));
  base::Time t2 = base::Time::Now();
  base::PlatformThread::Sleep(base::Milliseconds(kTimeoutMs));

  std::set<std::string> keys2 = {"6", "7"};
  CreateCacheEntries(keys2);

  base::PlatformThread::Sleep(base::Milliseconds(kTimeoutMs));
  base::Time t3 = base::Time::Now();

  // Count all entries.
  CountEntries(t1, t3);
  WaitForTasksOnIOThread();
  int64_t size_1_3 = GetResult();

  // Count everything
  CountEntries(base::Time(), base::Time::Max());
  WaitForTasksOnIOThread();
  EXPECT_EQ(size_1_3, GetResult());

  // Count the size of the first set of entries.
  CountEntries(t1, t2);
  WaitForTasksOnIOThread();
  int64_t size_1_2 = GetResult();

  // Count the size of the second set of entries.
  CountEntries(t2, t3);
  WaitForTasksOnIOThread();
  int64_t size_2_3 = GetResult();

  if (IsUpperLimit()) {
    EXPECT_EQ(size_1_2, size_1_3);
    EXPECT_EQ(size_2_3, size_1_3);
  } else {
    EXPECT_GT(size_1_2, 0);
    EXPECT_GT(size_2_3, 0);
    EXPECT_LT(size_1_2, size_1_3);
    EXPECT_LT(size_2_3, size_1_3);
    EXPECT_EQ(size_1_2 + size_2_3, size_1_3);
  }
}
