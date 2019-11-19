// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_fetcher.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using base::ASCIIToUTF16;

bool GetTestFilePath(const std::string& file_name, base::FilePath* path) {
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, path))
    return false;
  *path = path->AppendASCII("components")
              .AppendASCII("test")
              .AppendASCII("data")
              .AppendASCII("search_engines")
              .AppendASCII(file_name);
  return true;
}

class TestTemplateUrlFetcher : public TemplateURLFetcher {
 public:
  TestTemplateUrlFetcher(TemplateURLService* template_url_service,
                         const base::Closure& request_completed_callback)
      : TemplateURLFetcher(template_url_service),
        callback_(request_completed_callback) {}
  ~TestTemplateUrlFetcher() override {}

 protected:
  void RequestCompleted(RequestDelegate* request) override {
    callback_.Run();
    TemplateURLFetcher::RequestCompleted(request);
  }

 private:
  // Callback to be run when a request completes.
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TestTemplateUrlFetcher);
};

// Basic set-up for TemplateURLFetcher tests.
class TemplateURLFetcherTest : public testing::Test {
 public:
  TemplateURLFetcherTest();

  void SetUp() override {
    template_url_fetcher_.reset(new TestTemplateUrlFetcher(
        test_util_.model(),
        base::Bind(&TemplateURLFetcherTest::RequestCompletedCallback,
                   base::Unretained(this))));
  }

  // Called when a request completes.
  void RequestCompletedCallback();

  // Schedules the download of the url.
  void StartDownload(const base::string16& keyword,
                     const std::string& osdd_file_name,
                     bool check_that_file_exists);

  // Handles an incoming request.
  bool HandleRequest(content::URLLoaderInterceptor::RequestParams* params) {
    base::FilePath path;
    CHECK(GetTestFilePath(params->url_request.url.ExtractFileName(), &path));
    content::URLLoaderInterceptor::WriteResponse(path, params->client.get());
    return true;
  }

  // Waits for any downloads to finish.
  void WaitForDownloadToFinish();

  TemplateURLServiceTestUtil* test_util() { return &test_util_; }
  TemplateURLFetcher* template_url_fetcher() {
    return template_url_fetcher_.get();
  }
  int requests_completed() const { return requests_completed_; }

 private:
  content::BrowserTaskEnvironment
      task_environment_;  // To set up BrowserThreads.
  TemplateURLServiceTestUtil test_util_;
  std::unique_ptr<TemplateURLFetcher> template_url_fetcher_;
  content::URLLoaderInterceptor url_loader_interceptor_;

  // How many TemplateURKFetcher::RequestDelegate requests have completed.
  int requests_completed_;

  // Is the code in WaitForDownloadToFinish in a message loop waiting for a
  // callback to finish?
  bool waiting_for_download_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTest);
};

TemplateURLFetcherTest::TemplateURLFetcherTest()
    : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
      url_loader_interceptor_(
          base::BindRepeating(&TemplateURLFetcherTest::HandleRequest,
                              base::Unretained(this))),
      requests_completed_(0),
      waiting_for_download_(false) {}

void TemplateURLFetcherTest::RequestCompletedCallback() {
  requests_completed_++;
  if (waiting_for_download_)
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void TemplateURLFetcherTest::StartDownload(
    const base::string16& keyword,
    const std::string& osdd_file_name,
    bool check_that_file_exists) {
  if (check_that_file_exists) {
    base::FilePath osdd_full_path;
    ASSERT_TRUE(GetTestFilePath(osdd_file_name, &osdd_full_path));
    ASSERT_TRUE(base::PathExists(osdd_full_path));
    ASSERT_FALSE(base::DirectoryExists(osdd_full_path));
  }

  // Start the fetch.
  GURL osdd_url("http://some.url/" + osdd_file_name);
  GURL favicon_url;

  TestingProfile* profile = test_util_.profile();
  template_url_fetcher_->ScheduleDownload(
      keyword, osdd_url, favicon_url, url::Origin::Create(GURL()),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      0 /* render_frame_id */, 0 /* resource_type */);
}

void TemplateURLFetcherTest::WaitForDownloadToFinish() {
  ASSERT_FALSE(waiting_for_download_);
  waiting_for_download_ = true;
  base::RunLoop().Run();
  waiting_for_download_ = false;
}

TEST_F(TemplateURLFetcherTest, BasicAutodetectedTest) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(0, requests_completed());

  WaitForDownloadToFinish();
  EXPECT_EQ(1, requests_completed());

  const TemplateURL* t_url = test_util()->model()->GetTemplateURLForKeyword(
      keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            t_url->url_ref().DisplayURL(
                test_util()->model()->search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("Simple Search"), t_url->short_name());
  EXPECT_TRUE(t_url->safe_for_autoreplace());
}

// This test is similar to the BasicAutodetectedTest except the xml file
// provided doesn't include a short name for the search engine.  We should
// fall back to the hostname.
TEST_F(TemplateURLFetcherTest, InvalidShortName) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search_no_name.xml");
  StartDownload(keyword, osdd_file_name, true);
  WaitForDownloadToFinish();

  const TemplateURL* t_url =
      test_util()->model()->GetTemplateURLForKeyword(keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(ASCIIToUTF16("example.com"), t_url->short_name());
}

TEST_F(TemplateURLFetcherTest, DuplicatesThrownAway) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(0, requests_completed());

  struct {
    std::string description;
    std::string osdd_file_name;
    base::string16 keyword;
  } test_cases[] = {
      {"Duplicate osdd url with autodetected provider.", osdd_file_name,
       keyword + ASCIIToUTF16("1")},
      {"Duplicate keyword with autodetected provider.", osdd_file_name + "1",
       keyword},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    StartDownload(test_cases[i].keyword, test_cases[i].osdd_file_name, false);
    EXPECT_EQ(1, template_url_fetcher()->requests_count())
        << test_cases[i].description;
  }

  WaitForDownloadToFinish();
  EXPECT_EQ(1, requests_completed());
}

TEST_F(TemplateURLFetcherTest, AutodetectedBeforeLoadTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  EXPECT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  // This should bail because the model isn't loaded yet.
  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(0, template_url_fetcher()->requests_count());
  EXPECT_EQ(0, requests_completed());
}

TEST_F(TemplateURLFetcherTest, DuplicateKeywordsTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL("http://example.com/");
  test_util()->model()->Add(std::make_unique<TemplateURL>(data));
  test_util()->ChangeModelToLoadState();

  EXPECT_TRUE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  // This should bail because the keyword already exists.
  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(0, template_url_fetcher()->requests_count());
  EXPECT_EQ(0, requests_completed());
}

TEST_F(TemplateURLFetcherTest, DuplicateDownloadTest) {
  test_util()->ChangeModelToLoadState();

  base::string16 keyword(ASCIIToUTF16("test"));
  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(1, template_url_fetcher()->requests_count());
  EXPECT_EQ(0, requests_completed());

  // This should bail because the keyword already has a pending download.
  StartDownload(keyword, osdd_file_name, true);
  EXPECT_EQ(1, template_url_fetcher()->requests_count());
  EXPECT_EQ(0, requests_completed());

  WaitForDownloadToFinish();
  EXPECT_EQ(1, requests_completed());
}

TEST_F(TemplateURLFetcherTest, UnicodeTest) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("unicode_open_search.xml");
  StartDownload(keyword, osdd_file_name, true);
  WaitForDownloadToFinish();
  const TemplateURL* t_url =
      test_util()->model()->GetTemplateURLForKeyword(keyword);
  EXPECT_EQ(base::UTF8ToUTF16("\xd1\x82\xd0\xb5\xd1\x81\xd1\x82"),
            t_url->short_name());
}

}  // namespace
