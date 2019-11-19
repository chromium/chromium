// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class SiteDataCountingHelperTest : public testing::Test {
 public:
  const int64_t kTimeoutMs = 10;

  void SetUp() override {
    profile_.reset(new TestingProfile());
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateCookies(base::Time creation_time,
                     const std::vector<std::string>& urls) {
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(profile());
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
              net::CookieSameSite::NO_RESTRICTION,
              net::COOKIE_PRIORITY_DEFAULT);
      net::CookieOptions options;
      options.set_include_httponly();
      cookie_manager->SetCanonicalCookie(
          *cookie, url.scheme(), options,
          base::BindLambdaForTesting(
              [&](net::CanonicalCookie::CookieInclusionStatus status) {
                if (--tasks == 0)
                  run_loop.Quit();
              }));
    }

    run_loop.Run();
  }

  void CreateLocalStorage(
      base::Time creation_time,
      const std::vector<base::FilePath::StringPieceType>& storage_origins) {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path =
        profile()->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    // Write some files.
    for (const auto& origin : storage_origins) {
      base::WriteFile(storage_path.Append(origin), NULL, 0);
      base::TouchFile(storage_path.Append(origin), creation_time,
                      creation_time);
    }
  }

  int CountEntries(base::Time begin_time) {
    base::RunLoop run_loop;
    int result = -1;
    auto* helper = new SiteDataCountingHelper(
        profile(), begin_time, base::BindLambdaForTesting([&](int count) {
          // Negative values represent an unexpected error.
          DCHECK_GE(count, 0);
          result = count;
          run_loop.Quit();
        }));
    helper->CountAndDestroySelfWhenFinished();
    run_loop.Run();

    return result;
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SiteDataCountingHelperTest, CheckEmptyResult) {
  EXPECT_EQ(0, CountEntries(base::Time()));
}

TEST_F(SiteDataCountingHelperTest, CountCookies) {
  base::Time now = base::Time::Now();
  base::Time last_hour = now - base::TimeDelta::FromHours(1);
  base::Time yesterday = now - base::TimeDelta::FromDays(1);

  CreateCookies(last_hour, {"https://example.com"});
  CreateCookies(yesterday, {"https://google.com", "https://bing.com"});

  EXPECT_EQ(3, CountEntries(base::Time()));
  EXPECT_EQ(3, CountEntries(yesterday));
  EXPECT_EQ(1, CountEntries(last_hour));
  EXPECT_EQ(0, CountEntries(now));
}

TEST_F(SiteDataCountingHelperTest, LocalStorage) {
  base::Time now = base::Time::Now();
  CreateLocalStorage(now,
                     {FILE_PATH_LITERAL("https_example.com_443.localstorage"),
                      FILE_PATH_LITERAL("https_bing.com_443.localstorage")});

  EXPECT_EQ(2, CountEntries(base::Time()));
}

TEST_F(SiteDataCountingHelperTest, CookiesAndLocalStorage) {
  base::Time now = base::Time::Now();
  CreateCookies(now, {"http://example.com", "https://google.com"});
  CreateLocalStorage(now,
                     {FILE_PATH_LITERAL("https_example.com_443.localstorage"),
                      FILE_PATH_LITERAL("https_bing.com_443.localstorage")});

  EXPECT_EQ(3, CountEntries(base::Time()));
}

TEST_F(SiteDataCountingHelperTest, SameHostDifferentScheme) {
  base::Time now = base::Time::Now();
  CreateCookies(now, {"http://google.com", "https://google.com"});
  CreateLocalStorage(now,
                     {FILE_PATH_LITERAL("https_google.com_443.localstorage"),
                      FILE_PATH_LITERAL("http_google.com_80.localstorage")});

  EXPECT_EQ(1, CountEntries(base::Time()));
}
