// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_utils.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/mock_download_controller.h"
#endif

namespace offline_pages {
namespace {

const GURL kTestPage1Url("http://test.org/page1");
const GURL kTestPage2Url("http://test.org/page2");
const GURL kTestPage3Url("http://test.org/page3");
const GURL kTestPage4Url("http://test.org/page4");
const int64_t kTestFileSize = 876543LL;
const char* kTestPage1ClientId = "1234";
const char* kTestPage2ClientId = "5678";
const char* kTestPage3ClientId = "7890";
const char* kTestPage4ClientId = "42";

void CheckDuplicateDownloadsCallback(
    OfflinePageUtils::DuplicateCheckResult* out_result,
    OfflinePageUtils::DuplicateCheckResult result) {
  DCHECK(out_result);
  *out_result = result;
}

void GetAllRequestsCallback(
    std::vector<std::unique_ptr<SavePageRequest>>* out_requests,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  *out_requests = std::move(requests);
}

void SavePageLaterCallback(AddRequestResult ignored) {}

}  // namespace

class OfflinePageUtilsTest
    : public testing::Test,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageUtilsTest> {
 public:
  OfflinePageUtilsTest();
  ~OfflinePageUtilsTest() override;

  void SetUp() override;
  void TearDown() override;
  void RunUntilIdle();

  void SavePage(const GURL& url,
                const ClientId& client_id,
                std::unique_ptr<OfflinePageArchiver> archiver);

  // Return number of matches found.
  int FindRequestByNamespaceAndURL(const std::string& name_space,
                                   const GURL& url);

  // Necessary callbacks for the offline page model.
  void OnSavePageDone(SavePageResult result, int64_t offlineId);
  void OnClearAllDone();
  void OnExpirePageDone(bool success);
  void OnGetURLDone(const GURL& url);
  void OnSizeInBytesCalculated(int64_t size);

  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  TestingProfile* profile() { return &profile_; }
  content::WebContents* web_contents() const { return web_contents_.get(); }

  int64_t offline_id() const { return offline_id_; }
  int64_t last_cache_size() { return last_cache_size_; }

  void CreateCachedOfflinePages(base::SimpleTestClock* clock);

 private:
  void CreateOfflinePages();
  void CreateRequests();
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);

  content::TestBrowserThreadBundle browser_thread_bundle_;
  int64_t offline_id_;
  GURL url_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
  int64_t last_cache_size_;
#if defined(OS_ANDROID)
  chrome::android::MockDownloadController download_controller_;
#endif
};

OfflinePageUtilsTest::OfflinePageUtilsTest() = default;

OfflinePageUtilsTest::~OfflinePageUtilsTest() {}

void OfflinePageUtilsTest::SetUp() {
  // Create a test web contents.
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  OfflinePageTabHelper::CreateForWebContents(web_contents_.get());

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, base::BindRepeating(&BuildTestOfflinePageModel));
  RunUntilIdle();

  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, base::BindRepeating(&BuildTestRequestCoordinator));
  RunUntilIdle();

  // Make sure to create offline pages and requests.
  CreateOfflinePages();
  CreateRequests();

// This is needed in order to skip the logic to request storage permission.
#if defined(OS_ANDROID)
  DownloadControllerBase::SetDownloadControllerBase(&download_controller_);
#endif
}

void OfflinePageUtilsTest::TearDown() {
#if defined(OS_ANDROID)
  DownloadControllerBase::SetDownloadControllerBase(nullptr);
#endif
}

void OfflinePageUtilsTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageUtilsTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver), web_contents_.get(),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
}

void OfflinePageUtilsTest::OnSavePageDone(SavePageResult result,
                                          int64_t offline_id) {
  offline_id_ = offline_id;
}

void OfflinePageUtilsTest::OnExpirePageDone(bool success) {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnClearAllDone() {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnGetURLDone(const GURL& url) {
  url_ = url;
}

void OfflinePageUtilsTest::OnSizeInBytesCalculated(int64_t size) {
  last_cache_size_ = size;
}

void OfflinePageUtilsTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {}

void OfflinePageUtilsTest::CreateOfflinePages() {
  // Create page 1.
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPage1Url, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  offline_pages::ClientId client_id;
  client_id.name_space = kDownloadNamespace;
  client_id.id = kTestPage1ClientId;
  SavePage(kTestPage1Url, client_id, std::move(archiver));

  // Create page 2.
  archiver = BuildArchiver(kTestPage2Url,
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  SavePage(kTestPage2Url, client_id, std::move(archiver));
}

void OfflinePageUtilsTest::CreateRequests() {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(profile());

  RequestCoordinator::SavePageLaterParams params;
  params.url = kTestPage3Url;
  params.client_id =
      offline_pages::ClientId(kDownloadNamespace, kTestPage3ClientId);
  request_coordinator->SavePageLater(params,
                                     base::Bind(&SavePageLaterCallback));
  RunUntilIdle();
}

void OfflinePageUtilsTest::CreateCachedOfflinePages(
    base::SimpleTestClock* clock) {
  // Add 4 temporary pages to the model used for test cases. And setting current
  // time as the 00:00:00 time anchor.
  offline_pages::ClientId client_id;
  client_id.name_space = kBookmarkNamespace;

  clock->SetNow(base::Time::Now());
  // Time 01:00:00.
  clock->Advance(base::TimeDelta::FromHours(1));
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPage1Url, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  client_id.id = kTestPage1ClientId;
  SavePage(kTestPage1Url, client_id, std::move(archiver));
  // time 02:00:00.
  clock->Advance(base::TimeDelta::FromHours(1));
  archiver = BuildArchiver(kTestPage2Url,
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  SavePage(kTestPage2Url, client_id, std::move(archiver));
  // time 03:00:00.
  clock->Advance(base::TimeDelta::FromHours(1));
  archiver = BuildArchiver(kTestPage3Url,
                           base::FilePath(FILE_PATH_LITERAL("page3.mhtml")));
  client_id.id = kTestPage3ClientId;
  SavePage(kTestPage3Url, client_id, std::move(archiver));
  // Add a temporary page to test boundary at 10:00:00.
  clock->Advance(base::TimeDelta::FromHours(7));
  archiver = BuildArchiver(kTestPage4Url,
                           base::FilePath(FILE_PATH_LITERAL("page4.mhtml")));
  client_id.id = kTestPage4ClientId;
  SavePage(kTestPage4Url, client_id, std::move(archiver));
  // Reset clock->to 03:00:00.
  clock->SetNow(clock->Now() - base::TimeDelta::FromHours(7));
}

std::unique_ptr<OfflinePageTestArchiver> OfflinePageUtilsTest::BuildArchiver(
    const GURL& url,
    const base::FilePath& file_name) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      this, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      base::string16(), kTestFileSize, std::string(),
      base::ThreadTaskRunnerHandle::Get()));
  archiver->set_filename(file_name);
  return archiver;
}

int OfflinePageUtilsTest::FindRequestByNamespaceAndURL(
    const std::string& name_space,
    const GURL& url) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(profile());
  std::vector<std::unique_ptr<SavePageRequest>> requests;
  request_coordinator->GetAllRequests(
      base::Bind(&GetAllRequestsCallback, base::Unretained(&requests)));
  RunUntilIdle();

  int matches = 0;
  for (auto& request : requests) {
    if (request->url() == url &&
        request->client_id().name_space == name_space) {
      matches++;
    }
  }
  return matches;
}

TEST_F(OfflinePageUtilsTest, CheckDuplicateDownloads) {
  OfflinePageUtils::DuplicateCheckResult result =
      OfflinePageUtils::DuplicateCheckResult::NOT_FOUND;

  // The duplicate page should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage1Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_PAGE_FOUND,
            result);

  // The duplicate request should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage3Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND,
            result);

  // No duplicate should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage4Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::NOT_FOUND, result);
}

TEST_F(OfflinePageUtilsTest, ScheduleDownload) {
  // Pre-check.
  ASSERT_EQ(0, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage1Url));
  ASSERT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage3Url));
  ASSERT_EQ(0, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage4Url));

  // Re-downloading a page with duplicate page found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage1Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage1Url));

  // Re-downloading a page with duplicate request found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage3Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(2, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage3Url));

  // Downloading a page with no duplicate found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage4Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage4Url));
}

#if defined(OS_ANDROID)
TEST_F(OfflinePageUtilsTest, ScheduleDownloadWithFailedFileAcecssRequest) {
  DownloadControllerBase::Get()->SetApproveFileAccessRequestForTesting(false);
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage4Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(0, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage4Url));
}
#endif

TEST_F(OfflinePageUtilsTest, EqualsIgnoringFragment) {
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://example.com/")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://example.com/#test")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/#test"), GURL("http://example.com/")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/#test"), GURL("http://example.com/#test2")));
  EXPECT_FALSE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://test.com/#test")));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeBetween) {
  // Set a test clock before adding cached offline pages.
  // The clock will be at 03:00:00 after adding pages.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);
  CreateCachedOfflinePages(&clock);

  // Advance the clock so that we don't hit the time check boundary.
  clock.Advance(base::TimeDelta::FromMinutes(5));

  // Get the size of cached offline pages between 01:05:00 and 03:05:00.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now() - base::TimeDelta::FromHours(2), clock.Now());
  RunUntilIdle();
  EXPECT_TRUE(ret);
  EXPECT_EQ(kTestFileSize * 2, last_cache_size());
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeNoPageInModel) {
  // Set a test clock.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);

  clock.Advance(base::TimeDelta::FromHours(3));

  // Get the size of cached offline pages between 01:00:00 and 03:00:00.
  // Since no temporary pages were added to the model, the cache size should be
  // 0.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now() - base::TimeDelta::FromHours(2), clock.Now());
  RunUntilIdle();
  EXPECT_TRUE(ret);
  EXPECT_EQ(0, last_cache_size());
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeNoPageInRange) {
  // Set a test clock before adding cached offline pages.
  // The clock will be at 03:00:00 after adding pages.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);
  CreateCachedOfflinePages(&clock);

  // Advance the clock so that we don't hit the time check boundary.
  clock.Advance(base::TimeDelta::FromMinutes(5));

  // Get the size of cached offline pages between 03:04:00 and 03:05:00.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now() - base::TimeDelta::FromMinutes(1), clock.Now());
  RunUntilIdle();
  EXPECT_TRUE(ret);
  EXPECT_EQ(0, last_cache_size());
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeAllPagesInRange) {
  // Set a test clock before adding cached offline pages.
  // The clock will be at 03:00:00 after adding pages.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);
  CreateCachedOfflinePages(&clock);

  // Advance the clock to 23:00:00.
  clock.Advance(base::TimeDelta::FromHours(20));

  // Get the size of cached offline pages between -01:00:00 and 23:00:00.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now() - base::TimeDelta::FromHours(24), clock.Now());
  RunUntilIdle();
  EXPECT_TRUE(ret);
  EXPECT_EQ(kTestFileSize * 4, last_cache_size());
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeAllPagesInvalidRange) {
  // Set a test clock before adding cached offline pages.
  // The clock will be at 03:00:00 after adding pages.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);
  CreateCachedOfflinePages(&clock);

  // Advance the clock to 23:00:00.
  clock.Advance(base::TimeDelta::FromHours(20));

  // Get the size of cached offline pages between 23:00:00 and -01:00:00, which
  // is an invalid range, the return value will be false and there will be no
  // callback.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now(), clock.Now() - base::TimeDelta::FromHours(24));
  RunUntilIdle();
  EXPECT_FALSE(ret);
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeEdgeCase) {
  // Set a test clock before adding cached offline pages.
  // The clock will be at 03:00:00 after adding pages.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());
  base::SimpleTestClock clock;
  static_cast<OfflinePageModelTaskified*>(model)->SetClockForTesting(&clock);
  CreateCachedOfflinePages(&clock);

  // Get the size of cached offline pages between 02:00:00 and 03:00:00, since
  // we are using a [begin_time, end_time) range so there will be only 1 page
  // when query for this time range.
  bool ret = OfflinePageUtils::GetCachedOfflinePageSizeBetween(
      profile(),
      base::Bind(&OfflinePageUtilsTest::OnSizeInBytesCalculated, AsWeakPtr()),
      clock.Now() - base::TimeDelta::FromHours(1), clock.Now());
  RunUntilIdle();
  EXPECT_TRUE(ret);
  EXPECT_EQ(kTestFileSize * 1, last_cache_size());
}

TEST_F(OfflinePageUtilsTest, TestExtractOfflineHeaderValueFromNavigationEntry) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  std::string header_value;

  // Expect empty string if no header is present.
  header_value =
      OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(*entry);
  EXPECT_EQ("", header_value);

  // Expect correct header value for correct header format.
  entry->AddExtraHeaders("X-Chrome-offline: foo");
  header_value =
      OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(*entry);
  EXPECT_EQ("foo", header_value);

  // Expect empty string if multiple headers are set.
  entry->AddExtraHeaders("Another-Header: bar");
  header_value =
      OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(*entry);
  EXPECT_EQ("", header_value);

  // Expect empty string for incorrect header format.
  entry = content::NavigationEntry::Create();
  entry->AddExtraHeaders("Random value");
  header_value =
      OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(*entry);
  EXPECT_EQ("", header_value);
}

}  // namespace offline_pages
