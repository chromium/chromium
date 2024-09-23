// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_mock_clock_override.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_access_result.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

class SiteDataCountingHelperTest : public testing::Test {
 public:
  const int64_t kTimeoutMs = 10;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateCookies(base::Time creation_time,
                     const std::vector<std::string>& urls) {
    content::StoragePartition* partition =
        profile()->GetDefaultStoragePartition();
    network::mojom::CookieManager* cookie_manager =
        partition->GetCookieManagerForBrowserProcess();

    base::RunLoop run_loop;
    int tasks = urls.size();

    for (const std::string& url_string : urls) {
      GURL url(url_string);
      std::unique_ptr<net::CanonicalCookie> cookie =
          net::CanonicalCookie::CreateSanitizedCookie(
              url, "name", "A=1", url.host(), url.path(), creation_time,
              base::Time(), creation_time, url.SchemeIsCryptographic(), false,
              net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
              std::nullopt, /*status=*/nullptr);
      net::CookieOptions options;
      options.set_include_httponly();
      cookie_manager->SetCanonicalCookie(
          *cookie, url, options,
          base::BindLambdaForTesting(
              [&](net::CookieAccessResult access_result) {
                if (--tasks == 0)
                  run_loop.Quit();
              }));
    }

    run_loop.Run();
  }

  // Creates local storage data with a last-modified time given by the return
  // value of base::Time::Now().
  void CreateLocalStorage(const std::vector<std::string>& storage_origins) {
    storage::mojom::LocalStorageControl* local_storage_control =
        profile()->GetDefaultStoragePartition()->GetLocalStorageControl();

    for (const std::string& origin_str : storage_origins) {
      blink::StorageKey storage_key =
          blink::StorageKey::CreateFromStringForTesting(origin_str);
      ASSERT_FALSE(storage_key.origin().opaque());
      mojo::Remote<blink::mojom::StorageArea> area;
      local_storage_control->BindStorageArea(storage_key,
                                             area.BindNewPipeAndPassReceiver());

      bool success = false;
      base::RunLoop put_run_loop;
      area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, std::nullopt,
                "source", base::BindLambdaForTesting([&](bool success_in) {
                  success = success_in;
                  put_run_loop.Quit();
                }));
      put_run_loop.Run();
      ASSERT_TRUE(success);
    }
  }

  int CountEntries(base::Time begin_time, base::Time end_time) {
    base::RunLoop run_loop;
    int result = -1;
    auto* helper =
        new SiteDataCountingHelper(profile(), begin_time, end_time,
                                   base::BindLambdaForTesting([&](int count) {
                                     // Negative values represent an unexpected
                                     // error.
                                     DCHECK_GE(count, 0);
                                     result = count;
                                     run_loop.Quit();
                                   }));
    helper->CountAndDestroySelfWhenFinished();
    run_loop.Run();

    return result;
  }

  Profile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SiteDataCountingHelperTest, CheckEmptyResult) {
  EXPECT_EQ(0, CountEntries(base::Time(), base::Time::Max()));
}

TEST_F(SiteDataCountingHelperTest, CountCookies) {
  base::Time now = base::Time::Now();
  base::Time last_hour = now - base::Hours(1);
  base::Time yesterday = now - base::Days(1);

  CreateCookies(last_hour, {"https://example.com"});
  CreateCookies(yesterday, {"https://google.com", "https://bing.com"});

  EXPECT_EQ(3, CountEntries(base::Time(), now));
  EXPECT_EQ(0, CountEntries(base::Time(), yesterday));
  EXPECT_EQ(2, CountEntries(base::Time(), last_hour));
  EXPECT_EQ(2, CountEntries(yesterday, last_hour));
  EXPECT_EQ(3, CountEntries(yesterday, now));
  EXPECT_EQ(1, CountEntries(last_hour, now));
  EXPECT_EQ(0, CountEntries(now, now));
  EXPECT_EQ(0, CountEntries(now, base::Time::Max()));
}

TEST_F(SiteDataCountingHelperTest, LocalStorage) {
  // Set data "one day ago".
  CreateLocalStorage({"https://example.com"});

  // Advance time and spin the task queue so that local storage commits data.
  // Until the data is committed to disk, it will count as "now".
  task_environment_.AdvanceClock(base::Days(1));
  base::RunLoop().RunUntilIdle();

  // Advance time and set more data "now".
  task_environment_.AdvanceClock(base::Days(1));
  CreateLocalStorage({"https://bing.com"});

  base::Time now = base::Time::Now();
  base::Time last_hour = now - base::Hours(1);
  base::Time two_days_ago = now - base::Days(2);

  EXPECT_EQ(1, CountEntries(base::Time(), last_hour));
  EXPECT_EQ(1, CountEntries(last_hour, base::Time::Max()));
  EXPECT_EQ(2, CountEntries(base::Time(), base::Time::Max()));
  EXPECT_EQ(0, CountEntries(base::Time(), two_days_ago));
  EXPECT_EQ(1, CountEntries(two_days_ago, last_hour));
  EXPECT_EQ(2, CountEntries(two_days_ago, base::Time::Max()));
}

TEST_F(SiteDataCountingHelperTest, CookiesAndLocalStorage) {
  base::Time now = base::Time::Now();
  CreateCookies(now, {"http://example.com", "https://google.com"});
  CreateLocalStorage({"https://example.com", "https://bing.com"});

  EXPECT_EQ(3, CountEntries(base::Time(), base::Time::Max()));
}

TEST_F(SiteDataCountingHelperTest, SameHostDifferentScheme) {
  base::Time now = base::Time::Now();
  CreateCookies(now, {"http://google.com", "https://google.com"});
  CreateLocalStorage({"https://google.com", "http://google.com"});

  EXPECT_EQ(1, CountEntries(base::Time(), base::Time::Max()));
}
