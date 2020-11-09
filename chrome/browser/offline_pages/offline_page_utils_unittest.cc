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
#include "base/location.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/bind_test_util.h"
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
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/test/test_timeouts.h"
#include "chrome/browser/download/android/mock_download_controller.h"
#include "components/gcm_driver/instance_id/instance_id_android.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#endif

namespace offline_pages {
namespace {

const int64_t kTestFileSize = 876543LL;
const char* kTestPage1ClientId = "1234";
const char* kTestPage2ClientId = "5678";
const char* kTestPage3ClientId = "7890";
const char* kTestPage4ClientId = "42";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL TestPage1Url() {
  return GURL("http://test.org/page1");
}
GURL TestPage2Url() {
  return GURL("http://test.org/page2");
}
GURL TestPage3Url() {
  return GURL("http://test.org/page3");
}
GURL TestPage4Url() {
  return GURL("http://test.org/page4");
}

void RunTasksForDuration(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

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

  void SavePage(const GURL& url,
                const ClientId& client_id,
                std::unique_ptr<OfflinePageArchiver> archiver);

  // Return number of matches found.
  int FindRequestByNamespaceAndURL(const std::string& name_space,
                                   const GURL& url);

  size_t GetRequestCount() { return GetAllRequests().size(); }

  // Wait until there are at least |min_request_count| requests.
  void WaitForRequestMinCount(size_t min_request_count) {
    for (;;) {
      if (min_request_count <= GetRequestCount()) {
        break;
      }
      RunTasksForDuration(base::TimeDelta::FromMilliseconds(100));
    }
  }

  RequestCoordinator* GetRequestCoordinator() {
    return RequestCoordinatorFactory::GetForBrowserContext(profile());
  }

  OfflinePageUtils::DuplicateCheckResult CheckDuplicateDownloads(GURL url) {
    OfflinePageUtils::DuplicateCheckResult result;
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    auto on_done = [&](OfflinePageUtils::DuplicateCheckResult check_result) {
      result = check_result;
      quit.Run();
    };
    OfflinePageUtils::CheckDuplicateDownloads(
        profile(), url, base::BindLambdaForTesting(on_done));

    run_loop.Run();
    return result;
  }

  base::Optional<int64_t> GetCachedOfflinePageSizeBetween(
      const base::Time& begin_time,
      const base::Time& end_time) {
    int64_t result;
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    auto on_done = [&](int64_t size) {
      result = size;
      quit.Run();
    };
    if (!OfflinePageUtils::GetCachedOfflinePageSizeBetween(
            profile(), base::BindLambdaForTesting(on_done), begin_time,
            end_time)) {
      return base::nullopt;
    }
    run_loop.Run();
    return result;
  }

  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override {}

  TestScopedOfflineClock* clock() { return &clock_; }
  TestingProfile* profile() { return &profile_; }
  content::WebContents* web_contents() const { return web_contents_.get(); }

  void CreateCachedOfflinePages();
  std::vector<std::unique_ptr<SavePageRequest>> GetAllRequests() {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    std::vector<std::unique_ptr<SavePageRequest>> result;
    auto on_done = [&](std::vector<std::unique_ptr<SavePageRequest>> requests) {
      result = std::move(requests);
      quit.Run();
    };

    GetRequestCoordinator()->GetAllRequests(
        base::BindLambdaForTesting(on_done));
    run_loop.Run();
    return result;
  }

 private:
  void CreateOfflinePages();
  void CreateRequests();
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);

  TestScopedOfflineClock clock_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if defined(OS_ANDROID)
  chrome::android::MockDownloadController download_controller_;
  // OfflinePageTabHelper instantiates PrefetchService which in turn requests a
  // fresh GCM token automatically. This causes the request to be done
  // synchronously instead of with a posted task.
  instance_id::InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting
      block_async_;
#endif
};

OfflinePageUtilsTest::OfflinePageUtilsTest() = default;

OfflinePageUtilsTest::~OfflinePageUtilsTest() {}

void OfflinePageUtilsTest::SetUp() {
  // Create a test web contents.
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  OfflinePageTabHelper::CreateForWebContents(web_contents_.get());
  // Reset the value of the test clock.
  clock_.SetNow(base::Time::Now());

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.GetProfileKey(),
      base::BindRepeating(&BuildTestOfflinePageModel));

  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, base::BindRepeating(&BuildTestRequestCoordinator));

  // Make sure to create offline pages and requests.
  CreateOfflinePages();
  // TODO(harringtond): I was surprised this test creates requests in Setup(),
  // we should avoid this to be less surprising.
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

void OfflinePageUtilsTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  base::RunLoop run_loop;
  auto save_page_done = [&](SavePageResult result, int64_t offline_id) {
    run_loop.QuitClosure().Run();
  };
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver), web_contents_.get(),
      base::BindLambdaForTesting(save_page_done));
  run_loop.Run();
}

void OfflinePageUtilsTest::CreateOfflinePages() {
  // Create page 1.
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      TestPage1Url(), base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  offline_pages::ClientId client_id;
  client_id.name_space = kDownloadNamespace;
  client_id.id = kTestPage1ClientId;
  SavePage(TestPage1Url(), client_id, std::move(archiver));

  // Create page 2.
  archiver = BuildArchiver(TestPage2Url(),
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  SavePage(TestPage2Url(), client_id, std::move(archiver));
}

void OfflinePageUtilsTest::CreateRequests() {
  RequestCoordinator::SavePageLaterParams params;
  params.url = TestPage3Url();
  params.client_id =
      offline_pages::ClientId(kDownloadNamespace, kTestPage3ClientId);
  base::RunLoop run_loop;
  auto quit = run_loop.QuitClosure();
  auto page_saved = [&](AddRequestResult ignored) { quit.Run(); };
  GetRequestCoordinator()->SavePageLater(
      params, base::BindLambdaForTesting(page_saved));
  run_loop.Run();
}

void OfflinePageUtilsTest::CreateCachedOfflinePages() {
  // Add 4 temporary pages to the model used for test cases. And setting current
  // time as the 00:00:00 time anchor.
  offline_pages::ClientId client_id;
  client_id.name_space = kBookmarkNamespace;

  clock()->SetNow(base::Time::Now());
  // Time 01:00:00.
  clock()->Advance(base::TimeDelta::FromHours(1));
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      TestPage1Url(), base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  client_id.id = kTestPage1ClientId;
  SavePage(TestPage1Url(), client_id, std::move(archiver));
  // time 02:00:00.
  clock()->Advance(base::TimeDelta::FromHours(1));
  archiver = BuildArchiver(TestPage2Url(),
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  SavePage(TestPage2Url(), client_id, std::move(archiver));
  // time 03:00:00.
  clock()->Advance(base::TimeDelta::FromHours(1));
  archiver = BuildArchiver(TestPage3Url(),
                           base::FilePath(FILE_PATH_LITERAL("page3.mhtml")));
  client_id.id = kTestPage3ClientId;
  SavePage(TestPage3Url(), client_id, std::move(archiver));
  // Add a temporary page to test boundary at 10:00:00.
  clock()->Advance(base::TimeDelta::FromHours(7));
  archiver = BuildArchiver(TestPage4Url(),
                           base::FilePath(FILE_PATH_LITERAL("page4.mhtml")));
  client_id.id = kTestPage4ClientId;
  SavePage(TestPage4Url(), client_id, std::move(archiver));
  // Reset clock->to 03:00:00.
  clock()->Advance(base::TimeDelta::FromHours(-7));
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
  std::vector<std::unique_ptr<SavePageRequest>> requests = GetAllRequests();

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
  // The duplicate page should be found for this.
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_PAGE_FOUND,
            CheckDuplicateDownloads(TestPage1Url()));

  // The duplicate request should be found for this.
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND,
            CheckDuplicateDownloads(TestPage3Url()));

  // No duplicate should be found for this.
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::NOT_FOUND,
            CheckDuplicateDownloads(TestPage4Url()));
}

TEST_F(OfflinePageUtilsTest, ScheduleDownload) {
  // Pre-check.
  ASSERT_EQ(0,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage1Url()));
  ASSERT_EQ(1,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage3Url()));
  ASSERT_EQ(0,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage4Url()));

  // TODO(harringtond): Remove request creation in Setup().
  size_t request_count_wait = 1;
  // Re-downloading a page with duplicate page found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, TestPage1Url(),
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  WaitForRequestMinCount(++request_count_wait);
  EXPECT_EQ(1,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage1Url()));

  // Re-downloading a page with duplicate request found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, TestPage3Url(),
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  WaitForRequestMinCount(++request_count_wait);
  EXPECT_EQ(2,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage3Url()));

  // Downloading a page with no duplicate found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, TestPage4Url(),
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  WaitForRequestMinCount(++request_count_wait);
  EXPECT_EQ(1,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage4Url()));
}

#if defined(OS_ANDROID)
TEST_F(OfflinePageUtilsTest, ScheduleDownloadWithFailedFileAcecssRequest) {
  DownloadControllerBase::Get()->SetApproveFileAccessRequestForTesting(false);
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, TestPage4Url(),
      OfflinePageUtils::DownloadUIActionFlags::NONE);

  // Here, we're waiting to make sure a request is not created. We can't use
  // QuitClosure, since there's no callback threaded through ScheduleDownload.
  // Instead, just wait a bit and assume ScheduleDownload is complete.
  RunTasksForDuration(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(0,
            FindRequestByNamespaceAndURL(kDownloadNamespace, TestPage4Url()));
}
#endif

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeBetween) {
  // The clock will be at 03:00:00 after adding pages.
  CreateCachedOfflinePages();

  // Advance the clock so that we don't hit the time check boundary.
  clock()->Advance(base::TimeDelta::FromMinutes(5));

  // Get the size of cached offline pages between 01:05:00 and 03:05:00.
  EXPECT_EQ(
      kTestFileSize * 2,
      GetCachedOfflinePageSizeBetween(
          clock()->Now() - base::TimeDelta::FromHours(2), clock()->Now()));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeNoPageInModel) {
#if defined(OS_ANDROID)
  // TODO(https://crbug.com/1002762): Fix this test to run in < action_timeout()
  // on the Android bots.
  const base::test::ScopedRunLoopTimeout increased_run_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());
#endif  // defined(OS_ANDROID)

  clock()->Advance(base::TimeDelta::FromHours(3));

  // Get the size of cached offline pages between 01:00:00 and 03:00:00.
  // Since no temporary pages were added to the model, the cache size should be
  // 0.
  EXPECT_EQ(
      0, GetCachedOfflinePageSizeBetween(
             clock()->Now() - base::TimeDelta::FromHours(2), clock()->Now()));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeNoPageInRange) {
  // The clock will be at 03:00:00 after adding pages.
  CreateCachedOfflinePages();

  // Advance the clock so that we don't hit the time check boundary.
  clock()->Advance(base::TimeDelta::FromMinutes(5));

  // Get the size of cached offline pages between 03:04:00 and 03:05:00.
  EXPECT_EQ(
      0, GetCachedOfflinePageSizeBetween(
             clock()->Now() - base::TimeDelta::FromMinutes(1), clock()->Now()));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeAllPagesInRange) {
  // The clock will be at 03:00:00 after adding pages.
  CreateCachedOfflinePages();

  // Advance the clock to 23:00:00.
  clock()->Advance(base::TimeDelta::FromHours(20));

  // Get the size of cached offline pages between -01:00:00 and 23:00:00.
  EXPECT_EQ(
      kTestFileSize * 4,
      GetCachedOfflinePageSizeBetween(
          clock()->Now() - base::TimeDelta::FromHours(24), clock()->Now()));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeAllPagesInvalidRange) {
  // The clock will be at 03:00:00 after adding pages.
  CreateCachedOfflinePages();

  // Advance the clock to 23:00:00.
  clock()->Advance(base::TimeDelta::FromHours(20));

  // Get the size of cached offline pages between 23:00:00 and -01:00:00, which
  // is an invalid range, the return value will be false and there will be no
  // callback.
  EXPECT_FALSE(GetCachedOfflinePageSizeBetween(
      clock()->Now(), clock()->Now() - base::TimeDelta::FromHours(24)));
}

TEST_F(OfflinePageUtilsTest, TestGetCachedOfflinePageSizeEdgeCase) {
  // The clock will be at 03:00:00 after adding pages.
  CreateCachedOfflinePages();

  // Get the size of cached offline pages between 02:00:00 and 03:00:00, since
  // we are using a [begin_time, end_time) range so there will be only 1 page
  // when query for this time range.
  EXPECT_EQ(
      kTestFileSize * 1,
      GetCachedOfflinePageSizeBetween(
          clock()->Now() - base::TimeDelta::FromHours(1), clock()->Now()));
}

// Timeout on Android.  http://crbug.com/981972
#if defined(OS_ANDROID)
#define MAYBE_TestExtractOfflineHeaderValueFromNavigationEntry \
  DISABLED_TestExtractOfflineHeaderValueFromNavigationEntry
#else
#define MAYBE_TestExtractOfflineHeaderValueFromNavigationEntry \
  TestExtractOfflineHeaderValueFromNavigationEntry
#endif
TEST_F(OfflinePageUtilsTest,
       MAYBE_TestExtractOfflineHeaderValueFromNavigationEntry) {
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  std::string header_value;

  // Expect empty string if no header is present.
  header_value = OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(
      entry.get());
  EXPECT_EQ("", header_value);

  // Expect correct header value for correct header format.
  entry->AddExtraHeaders("X-Chrome-offline: foo");
  header_value = OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(
      entry.get());
  EXPECT_EQ("foo", header_value);

  // Expect empty string if multiple headers are set.
  entry->AddExtraHeaders("Another-Header: bar");
  header_value = OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(
      entry.get());
  EXPECT_EQ("", header_value);

  // Expect empty string for incorrect header format.
  entry = content::NavigationEntry::Create();
  entry->AddExtraHeaders("Random value");
  header_value = OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(
      entry.get());
  EXPECT_EQ("", header_value);
}

}  // namespace offline_pages
