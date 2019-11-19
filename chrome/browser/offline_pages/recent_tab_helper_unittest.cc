// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/recent_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
using IsSavingSamePageEnum = RecentTabHelper::IsSavingSamePageEnum;
const GURL kTestPageUrl("http://mystery.site/foo.html");
const GURL kTestPageUrlOther("http://crazy.site/foo_other.html");
const int kTabId = 153;
}  // namespace

class TestDelegate: public RecentTabHelper::Delegate {
 public:
  const size_t kArchiveSizeToReport = 1234;

  explicit TestDelegate(
      OfflinePageTestArchiver::Observer* observer,
      int tab_id,
      bool tab_id_result);
  ~TestDelegate() override {}

  std::unique_ptr<OfflinePageArchiver> CreatePageArchiver(
        content::WebContents* web_contents) override;
    // There is no expectations that tab_id is always present.
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override;
  bool IsLowEndDevice() override { return is_low_end_device_; }
  bool IsCustomTab(content::WebContents* web_contents) override {
    return is_custom_tab_;
  }

  void set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult result) {
    archive_result_ = result;
  }

  void set_archive_size(int64_t size) { archive_size_ = size; }

  void SetAsLowEndDevice() { is_low_end_device_ = true; }

  void set_is_custom_tab(bool is_custom_tab) { is_custom_tab_ = is_custom_tab; }

 private:
  OfflinePageTestArchiver::Observer* observer_;  // observer owns this.
  int tab_id_;
  bool tab_id_result_;

  // These values can be updated so that new OfflinePageTestArchiver instances
  // will return different results.
  offline_pages::OfflinePageArchiver::ArchiverResult archive_result_ =
      offline_pages::OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED;
  int64_t archive_size_ = kArchiveSizeToReport;
  bool is_low_end_device_ = false;
  bool is_custom_tab_ = false;
};

class RecentTabHelperTest
    : public ChromeRenderViewHostTestHarness,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer {
 public:
  RecentTabHelperTest();
  ~RecentTabHelperTest() override {}

  void SetUp() override;
  void TearDown() override;
  const std::vector<OfflinePageItem>& GetAllPages();

  void FailLoad(const GURL& url);

  // Runs main thread.
  void RunUntilIdle();
  // Advances main thread time to trigger the snapshot controller's timeouts.
  void FastForwardSnapshotController();

  void NavigateAndCommit(const GURL& url);
  void Reload();

  // Navigates to the URL and commit as if it has been typed in the address bar.
  // Note: we need this to simulate navigations to the same URL that more like a
  // reload and not same page. NavigateAndCommit simulates a click on a link
  // and when reusing the same URL that will be considered a same page
  // navigation.
  void NavigateAndCommitTyped(const GURL& url);

  // Navigates to the URL and commit as if a form had been submitted.
  void NavigateAndCommitPost(const GURL& url);

  ClientId NewDownloadClientId();

  RecentTabHelper* recent_tab_helper() const { return recent_tab_helper_; }

  OfflinePageModel* model() const { return model_; }

  TestDelegate* default_test_delegate() { return default_test_delegate_; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  // Returns a OfflinePageItem pointer from |all_pages| that matches the
  // provided |offline_id|. If a match is not found returns nullptr.
  const OfflinePageItem* FindPageForOfflineId(int64_t offline_id) {
    for (const OfflinePageItem& page : GetAllPages()) {
      if (page.offline_id == offline_id)
        return &page;
    }
    return nullptr;
  }

  size_t page_added_count() { return page_added_count_; }
  size_t model_removed_count() { return model_removed_count_; }

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override {
    all_pages_needs_updating_ = true;
  }
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override {
    page_added_count_++;
    all_pages_needs_updating_ = true;
  }
  void OfflinePageDeleted(const OfflinePageItem& item) override {
    model_removed_count_++;
    all_pages_needs_updating_ = true;
  }

  // OfflinePageTestArchiver::Observer
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override {}

 private:
  void StartAndCommitNavigation(
      std::unique_ptr<content::NavigationSimulator> simulator);

  void OnGetAllPagesDone(const std::vector<OfflinePageItem>& result);

  RecentTabHelper* recent_tab_helper_;   // Owned by WebContents.
  OfflinePageModel* model_;              // Keyed service.
  TestDelegate* default_test_delegate_;  // Created at SetUp.
  size_t page_added_count_;
  size_t model_removed_count_;
  std::vector<OfflinePageItem> all_pages_;
  bool all_pages_needs_updating_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Mocks the RenderViewHostTestHarness' main thread runner. Needs to be delay
  // initialized in SetUp() -- can't be a simple member -- since
  // RenderViewHostTestHarness only initializes its main thread environment in
  // its SetUp() :(.
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner>
      mocked_main_runner_;

  base::WeakPtrFactory<RecentTabHelperTest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelperTest);
};

TestDelegate::TestDelegate(
    OfflinePageTestArchiver::Observer* observer,
    int tab_id,
    bool tab_id_result)
    : observer_(observer),
      tab_id_(tab_id),
      tab_id_result_(tab_id_result) {
}

std::unique_ptr<OfflinePageArchiver> TestDelegate::CreatePageArchiver(
    content::WebContents* web_contents) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      observer_, web_contents->GetLastCommittedURL(), archive_result_,
      base::string16(), kArchiveSizeToReport, std::string(),
      base::ThreadTaskRunnerHandle::Get()));
  return std::move(archiver);
}

// There is no expectations that tab_id is always present.
bool TestDelegate::GetTabId(content::WebContents* web_contents, int* tab_id) {
  *tab_id = tab_id_;
  return tab_id_result_;
}

RecentTabHelperTest::RecentTabHelperTest()
    : recent_tab_helper_(nullptr),
      model_(nullptr),
      default_test_delegate_(nullptr),
      page_added_count_(0),
      model_removed_count_(0),
      all_pages_needs_updating_(true) {}

void RecentTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  mocked_main_runner_ =
      std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();

  // Sets up the factories for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile()->GetProfileKey(),
      base::BindRepeating(&BuildTestOfflinePageModel));
  RunUntilIdle();
  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating(&BuildTestRequestCoordinator));
  RunUntilIdle();

  RecentTabHelper::CreateForWebContents(web_contents());
  recent_tab_helper_ = RecentTabHelper::FromWebContents(web_contents());

  std::unique_ptr<TestDelegate> test_delegate(
      new TestDelegate(this, kTabId, true));
  default_test_delegate_ = test_delegate.get();
  recent_tab_helper_->SetDelegate(std::move(test_delegate));

  model_ = OfflinePageModelFactory::GetForBrowserContext(browser_context());
  model_->AddObserver(this);

  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void RecentTabHelperTest::TearDown() {
  mocked_main_runner_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

void RecentTabHelperTest::FailLoad(const GURL& url) {
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), url, net::ERR_INTERNET_DISCONNECTED);
}

const std::vector<OfflinePageItem>& RecentTabHelperTest::GetAllPages() {
  if (all_pages_needs_updating_) {
    model()->GetAllPages(base::Bind(&RecentTabHelperTest::OnGetAllPagesDone,
                                    weak_ptr_factory_.GetWeakPtr()));
    RunUntilIdle();
    all_pages_needs_updating_ = false;
  }
  return all_pages_;
}

void RecentTabHelperTest::OnGetAllPagesDone(
    const std::vector<OfflinePageItem>& result) {
  all_pages_ = result;
}

void RecentTabHelperTest::RunUntilIdle() {
  (*mocked_main_runner_)->RunUntilIdle();
}

void RecentTabHelperTest::FastForwardSnapshotController() {
  constexpr base::TimeDelta kLongDelay = base::TimeDelta::FromSeconds(100);
  (*mocked_main_runner_)->FastForwardBy(kLongDelay);
}

void RecentTabHelperTest::StartAndCommitNavigation(
    std::unique_ptr<content::NavigationSimulator> simulator) {
  simulator->SetAutoAdvance(false);
  simulator->SetKeepLoading(true);
  simulator->Start();

  // Need to flush the task queue manually since there may be async tasks
  // spawned by navigation start that must finish before commit. Since this test
  // harness swaps out the main thread, NavigationSimulator cannot pump the task
  // queue itself to finish navigations.
  //
  // TODO(csharrison): This can probably be removed and replaced with either the
  // NavigationSimulator controlling the mock task runner, or by the snapshot
  // controller using a (mock) timer instead of PostDelayedTask.
  RunUntilIdle();
  simulator->Commit();
}

void RecentTabHelperTest::NavigateAndCommit(const GURL& url) {
  StartAndCommitNavigation(content::NavigationSimulator::CreateBrowserInitiated(
      url, web_contents()));
}

void RecentTabHelperTest::Reload() {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      web_contents()->GetLastCommittedURL(), web_contents());
  simulator->SetReloadType(content::ReloadType::NORMAL);
  StartAndCommitNavigation(std::move(simulator));
}

void RecentTabHelperTest::NavigateAndCommitTyped(const GURL& url) {
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_TYPED);
  StartAndCommitNavigation(std::move(simulator));
}

void RecentTabHelperTest::NavigateAndCommitPost(const GURL& url) {
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetMethod("POST");
  simulator->SetTransition(ui::PAGE_TRANSITION_FORM_SUBMIT);
  StartAndCommitNavigation(std::move(simulator));
}

ClientId RecentTabHelperTest::NewDownloadClientId() {
  static int counter = 0;
  return ClientId(kDownloadNamespace,
                  std::string("id") + base::NumberToString(++counter));
}

// Checks the test setup.
TEST_F(RecentTabHelperTest, RecentTabHelperInstanceExists) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Init();
  EXPECT_NE(nullptr, recent_tab_helper());
}

// Fully loads a page then simulates the tab being hidden. Verifies that a
// snapshot is created only when the latter happens.
TEST_F(RecentTabHelperTest, LastNCaptureAfterLoad) {
  // Navigate and finish loading. Nothing should be saved.
  NavigateAndCommit(kTestPageUrl);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // Tab is hidden with a fully loaded page. A snapshot save should happen.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  EXPECT_EQ(kLastNNamespace, GetAllPages()[0].client_id.name_space);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);
}

// Simulates the tab being hidden too early in the page loading so that a
// snapshot should not be created.
TEST_F(RecentTabHelperTest, NoLastNCaptureIfTabHiddenTooEarlyInPageLoad) {
  // Commit the navigation and hide the tab. Nothing should be saved.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // Then allow the page to fully load. Nothing should be saved.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Checks that WebContents with no tab IDs have snapshot requests properly
// ignored from both last_n and downloads.
TEST_F(RecentTabHelperTest, NoTabIdNoCapture) {
  // Create delegate that returns 'false' as TabId retrieval result.
  recent_tab_helper()->SetDelegate(
      std::make_unique<TestDelegate>(this, kTabId, false));

  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  // No page should be captured.
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Checks that last_n is disabled if the device is low-end (aka svelte) but that
// download requests still work.
TEST_F(RecentTabHelperTest, LastNDisabledOnSvelte) {
  // Simulates a low end device.
  default_test_delegate()->SetAsLowEndDevice();

  // Navigate and finish loading then hide the tab. Nothing should be saved.
  NavigateAndCommit(kTestPageUrl);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // But the following download request should work normally
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Checks that last_n will not save a snapshot while the tab is being presented
// as a custom tab. Download requests should be unaffected though.
TEST_F(RecentTabHelperTest, LastNWontSaveCustomTab) {
  // Simulates the tab running as a custom tab.
  default_test_delegate()->set_is_custom_tab(true);

  // Navigate and finish loading then hide the tab. Nothing should be saved.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // But the following download request should work normally
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);

  // Simulates the tab being transfered from the CustomTabActivity back to a
  // ChromeActivity.
  default_test_delegate()->set_is_custom_tab(false);

  // Upon the next hide a last_n snapshot should be saved.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  ASSERT_EQ(2U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);
}

// Triggers two last_n snapshot captures during a single page load. Should end
// up with one snapshot, the 1st being replaced by the 2nd.
TEST_F(RecentTabHelperTest, TwoCapturesSamePageLoad) {
  NavigateAndCommit(kTestPageUrl);

  // Set page loading state to the 1st snapshot-able stage. No capture so far.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(0U, page_added_count());

  // Tab is hidden and a snapshot should be saved.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);
  int64_t first_offline_id = GetAllPages()[0].offline_id;

  // Set page loading state to the 2nd and last snapshot-able stage. No new
  // capture should happen.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());

  // Tab is hidden again. At this point a higher quality snapshot is expected so
  // a new one should be captured and replace the previous one.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  EXPECT_NE(first_offline_id, GetAllPages()[0].offline_id);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageBetterQuality, 1);
}

// Triggers two last_n captures during a single page load, where the 2nd capture
// fails. Should end up with one offline page (the 1st, successful snapshot
// should be kept).
// TODO(carlosk): re-enable once https://crbug.com/705079 is fixed.
TEST_F(RecentTabHelperTest, DISABLED_TwoCapturesWhere2ndFailsSamePageLoad) {
  // Navigate and load until the 1st stage. Tab hidden should trigger a capture.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  int64_t first_offline_id = GetAllPages()[0].offline_id;

  // Updates the delegate so that will make the second snapshot fail.
  default_test_delegate()->set_archive_size(-1);
  default_test_delegate()->set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult::
          ERROR_ARCHIVE_CREATION_FAILED);

  // Advance loading to the 2nd and final stage and then hide the tab. A new
  // capture is requested but its creation will fail. The exact same snapshot
  // from before should still be available.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  EXPECT_EQ(first_offline_id, GetAllPages()[0].offline_id);
}

// Triggers two last_n captures for two different loads of the same URL (aka
// reload). Should end up with a single snapshot (from the 2nd load).
TEST_F(RecentTabHelperTest, TwoCapturesDifferentPageLoadsSameUrl) {
  // Fully load the page. Hide the tab and check for a snapshot.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);
  int64_t first_offline_id = GetAllPages()[0].offline_id;

  // Reload the same URL until the page is minimally loaded. The previous
  // snapshot should have been removed.
  NavigateAndCommitTyped(kTestPageUrl);
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // Hide the tab and a new snapshot should be taken.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  EXPECT_NE(first_offline_id, GetAllPages()[0].offline_id);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 2);
}

// Triggers two last_n captures for two different page loads of the same URL
// (aka reload), where the 2nd capture fails. Should end up with no offline
// pages (a privacy driven decision).
TEST_F(RecentTabHelperTest, TwoCapturesWhere2ndFailsDifferentPageLoadsSameUrl) {
  // Fully load the page then hide the tab. A capture is expected.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  // Updates the delegate so that will make the second snapshot fail.
  default_test_delegate()->set_archive_size(-1);
  default_test_delegate()->set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult::
          ERROR_ARCHIVE_CREATION_FAILED);

  // Fully load the page once more then hide the tab again. A capture happens
  // and fails but no snapshot should remain.
  NavigateAndCommitTyped(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 2);
}

// Triggers two last_n captures for two different page loads of different URLs.
// Should end up with a single snapshot of the last page.
TEST_F(RecentTabHelperTest, TwoCapturesDifferentPageLoadsDifferentUrls) {
  // Fully load the first URL then hide the tab and check for a snapshot.
  NavigateAndCommit(kTestPageUrl);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  // Fully load the second URL. The previous snapshot should have been deleted.
  NavigateAndCommitTyped(kTestPageUrlOther);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // Then hide the tab and check for a single snapshot of the new page.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrlOther, GetAllPages()[0].url);
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 2);
}

// Fully loads a page where last_n captures two snapshots. Then triggers two
// snapshot requests by downloads. Should end up with three offline pages: one
// from last_n (2nd replaces the 1st) and two from downloads (which shouldn't
// replace each other).
TEST_F(RecentTabHelperTest, TwoLastNAndTwoDownloadCapturesSamePage) {
  // Fully loads the page with intermediary steps where the tab is hidden. Then
  // check that two last_n snapshots were created but only one was kept.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrl, GetAllPages()[0].url);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
  histogram_tester()->ExpectBucketCount("OfflinePages.LastN.IsSavingSamePage",
                                        IsSavingSamePageEnum::kNewPage, 1);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageBetterQuality, 1);
  int64_t first_offline_id = GetAllPages()[0].offline_id;

  // First snapshot request by downloads. Two offline pages are expected.
  const int64_t second_offline_id = first_offline_id + 1;
  const ClientId second_client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(second_client_id,
                                                     second_offline_id, "");
  RunUntilIdle();
  EXPECT_EQ(3U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(2U, GetAllPages().size());
  EXPECT_NE(nullptr, FindPageForOfflineId(first_offline_id));
  const OfflinePageItem* second_page = FindPageForOfflineId(second_offline_id);
  ASSERT_NE(nullptr, second_page);
  EXPECT_EQ(kTestPageUrl, second_page->url);
  EXPECT_EQ(second_client_id, second_page->client_id);

  // Second snapshot request by downloads. Three offline pages are expected.
  const int64_t third_offline_id = first_offline_id + 2;
  const ClientId third_client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(third_client_id,
                                                     third_offline_id, "");
  RunUntilIdle();
  EXPECT_EQ(4U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(3U, GetAllPages().size());
  EXPECT_NE(nullptr, FindPageForOfflineId(first_offline_id));
  EXPECT_NE(nullptr, FindPageForOfflineId(second_offline_id));
  const OfflinePageItem* third_page = FindPageForOfflineId(third_offline_id);
  ASSERT_NE(nullptr, third_page);
  EXPECT_EQ(kTestPageUrl, third_page->url);
  EXPECT_EQ(third_client_id, third_page->client_id);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
}

// Simulates an error (disconnection) during the load of a page. Should end up
// with no offline pages for any requester.
TEST_F(RecentTabHelperTest, NoCaptureOnErrorPage) {
  FailLoad(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Checks that last_n snapshots are not created if the feature is disabled.
// Download requests should still work.
TEST_F(RecentTabHelperTest, LastNFeatureNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kOffliningRecentPagesFeature);
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  // No page should be captured.
  ASSERT_EQ(0U, GetAllPages().size());

  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  // No page should be captured.
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates a download request to offline the current page made early during
// loading. Should execute two captures but only the final one is kept.
TEST_F(RecentTabHelperTest, DownloadRequestEarlyInLoad) {
  // Commit the navigation and request the snapshot from downloads. No captures
  // so far.
  NavigateAndCommit(kTestPageUrl);
  const ClientId client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id, 153L, "");
  FastForwardSnapshotController();
  ASSERT_EQ(0U, GetAllPages().size());

  // Minimally load the page. First capture should occur.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& early_page = GetAllPages()[0];
  EXPECT_EQ(kTestPageUrl, early_page.url);
  EXPECT_EQ(client_id, early_page.client_id);
  EXPECT_EQ(153L, early_page.offline_id);

  // Fully load the page. A second capture should replace the first one.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& later_page = GetAllPages()[0];
  EXPECT_EQ(kTestPageUrl, later_page.url);
  EXPECT_EQ(client_id, later_page.client_id);
  EXPECT_EQ(153L, later_page.offline_id);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates a download request to offline the current page made when the page
// is minimally loaded. Should execute two captures but only the final one is
// kept.
TEST_F(RecentTabHelperTest, DownloadRequestLaterInLoad) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  ASSERT_EQ(0U, GetAllPages().size());

  const ClientId client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id, 153L, "");
  RunUntilIdle();
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& page = GetAllPages()[0];
  EXPECT_EQ(kTestPageUrl, page.url);
  EXPECT_EQ(client_id, page.client_id);
  EXPECT_EQ(153L, page.offline_id);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates a download request to offline the current page made after loading
// is completed. Should end up with one offline page.
TEST_F(RecentTabHelperTest, DownloadRequestAfterFullyLoad) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  ASSERT_EQ(0U, GetAllPages().size());

  const ClientId client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id, 153L, "");
  RunUntilIdle();
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& page = GetAllPages()[0];
  EXPECT_EQ(kTestPageUrl, page.url);
  EXPECT_EQ(client_id, page.client_id);
  EXPECT_EQ(153L, page.offline_id);
  EXPECT_EQ("", page.request_origin);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates a download request to offline the current page made after loading
// is completed. Should end up with one offline page.
TEST_F(RecentTabHelperTest, DownloadRequestAfterFullyLoadWithOrigin) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  ASSERT_EQ(0U, GetAllPages().size());

  const ClientId client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id, 153L, "abc");
  RunUntilIdle();
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& page = GetAllPages()[0];
  EXPECT_EQ(kTestPageUrl, page.url);
  EXPECT_EQ(client_id, page.client_id);
  EXPECT_EQ(153L, page.offline_id);
  EXPECT_EQ("abc", page.request_origin);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates requests coming from last_n and downloads at the same time for a
// fully loaded page.
TEST_F(RecentTabHelperTest, SimultaneousCapturesFromLastNAndDownloads) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  const int64_t download_offline_id = 153L;
  const ClientId download_client_id = NewDownloadClientId();
  recent_tab_helper()->ObserveAndDownloadCurrentPage(download_client_id,
                                                     download_offline_id, "");
  RunUntilIdle();
  ASSERT_EQ(2U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  const OfflinePageItem* downloads_page =
      FindPageForOfflineId(download_offline_id);
  ASSERT_TRUE(downloads_page);
  EXPECT_EQ(kTestPageUrl, downloads_page->url);
  EXPECT_EQ(download_client_id, downloads_page->client_id);

  const OfflinePageItem& last_n_page =
      GetAllPages()[0].offline_id != download_offline_id ? GetAllPages()[0]
                                                         : GetAllPages()[1];
  EXPECT_EQ(kTestPageUrl, last_n_page.url);
  EXPECT_EQ(kLastNNamespace, last_n_page.client_id.name_space);
}

// Simulates multiple tab hidden events -- triggers for last_n snapshots --
// happening at the same loading stages. The duplicate events should create new
// snapshots (so that dynamic pages are properly persisted; navigation/loading
// signals are poor signals for those).
TEST_F(RecentTabHelperTest, DuplicateTabHiddenEventsShouldTriggerNewSnapshots) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentAvailableInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageSameQuality, 1);

  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(3U, page_added_count());
  EXPECT_EQ(2U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       3);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageBetterQuality, 1);

  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(4U, page_added_count());
  EXPECT_EQ(3U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       4);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageSameQuality, 2);
}

// Simulates multiple download requests and verifies that overlapping requests
// are ignored.
TEST_F(RecentTabHelperTest, OverlappingDownloadRequestsAreIgnored) {
  // Navigates and commits then make two download snapshot requests.
  NavigateAndCommit(kTestPageUrl);
  const ClientId client_id_1 = NewDownloadClientId();
  const int64_t offline_id_1 = 153L;
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id_1, offline_id_1,
                                                     "");
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     351L, "");

  // Finish loading the page. Only the first request should be executed.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  const OfflinePageItem& fist_page = GetAllPages()[0];
  EXPECT_EQ(client_id_1, fist_page.client_id);
  EXPECT_EQ(offline_id_1, fist_page.offline_id);

  // Make two additional download snapshot requests. Again only the first should
  // generate a snapshot.
  const ClientId client_id_3 = NewDownloadClientId();
  const int64_t offline_id_3 = 789L;
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id_3, offline_id_3,
                                                     "");
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     987L, "");
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(2U, GetAllPages().size());
  const OfflinePageItem* second_page = FindPageForOfflineId(offline_id_3);
  ASSERT_TRUE(second_page);
  EXPECT_EQ(client_id_3, second_page->client_id);
  EXPECT_EQ(offline_id_3, second_page->offline_id);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);
}

// Simulates a same document navigation and checks we snapshot correctly with
// last_n and downloads.
TEST_F(RecentTabHelperTest, SaveSameDocumentNavigationSnapshots) {
  // Navigates and load fully then hide the tab so that a snapshot is created.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  // Now navigates same page and check the results of hiding the tab again.
  // Another snapshot should be created to the updated URL.
  const GURL kTestPageUrlWithFragment(kTestPageUrl.spec() + "#aaa");
  NavigateAndCommit(kTestPageUrlWithFragment);
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  EXPECT_EQ(kTestPageUrlWithFragment, GetAllPages()[0].url);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
  histogram_tester()->ExpectBucketCount(
      "OfflinePages.LastN.IsSavingSamePage",
      IsSavingSamePageEnum::kSamePageSameQuality, 1);

  // Now create a download request and check the snapshot is properly created.
  const ClientId client_id = NewDownloadClientId();
  const int64_t offline_id = 153L;
  recent_tab_helper()->ObserveAndDownloadCurrentPage(client_id, offline_id, "");
  RunUntilIdle();
  EXPECT_EQ(3U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(2U, GetAllPages().size());
  const OfflinePageItem* downloads_page = FindPageForOfflineId(offline_id);
  EXPECT_EQ(kTestPageUrlWithFragment, downloads_page->url);
  EXPECT_EQ(client_id, downloads_page->client_id);
  EXPECT_EQ(offline_id, downloads_page->offline_id);
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       2);
}

// Tests that a page reloaded is tracked as an actual load and properly saved.
TEST_F(RecentTabHelperTest, ReloadIsTrackedAsNavigationAndSavedOnlyUponLoad) {
  // Navigates and load fully then hide the tab so that a snapshot is created.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);

  // Starts a reload and hides the tab before it minimally load. The previous
  // snapshot should be removed.
  Reload();

  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(0U, GetAllPages().size());

  // Finish loading and hide the tab. A new snapshot should be created.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 2);
}

// Checks that a closing tab doesn't trigger the creation of a snapshot. And
// also that if the closure is reverted, a snapshot is saved upon the next hide
// event.
TEST_F(RecentTabHelperTest, NoSaveIfTabIsClosing) {
  // Navigates and fully load then close and hide the tab. No snapshots are
  // expected.
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  // Note: These two next calls are always expected to happen in this order.
  recent_tab_helper()->WillCloseTab();
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(0U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);

  // Simulates the page being restored and shown again, then hidden. At this
  // moment a snapshot should be created.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::VISIBLE);
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  ASSERT_EQ(1U, GetAllPages().size());
  histogram_tester()->ExpectUniqueSample("OfflinePages.LastN.IsSavingSamePage",
                                         IsSavingSamePageEnum::kNewPage, 1);
}

TEST_F(RecentTabHelperTest, NoSaveOfflinePageCacheForPost) {
  // Navigate and finish loading, then move the snapshot controller's time
  // forward so it gets past timeouts. Nothing should be saved.
  NavigateAndCommitPost(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  ASSERT_EQ(0U, GetAllPages().size());

  // Tab is hidden with a fully loaded page. A snapshot save should not happen
  // due to the POST method - OfflinePageCache is disabled.
  recent_tab_helper()->OnVisibilityChanged(content::Visibility::HIDDEN);
  RunUntilIdle();
  EXPECT_EQ(0U, page_added_count());
  ASSERT_EQ(0U, GetAllPages().size());
  histogram_tester()->ExpectTotalCount("OfflinePages.LastN.IsSavingSamePage",
                                       0);

  // A manual download should succeed despite being ineligible for OPC.
  recent_tab_helper()->ObserveAndDownloadCurrentPage(NewDownloadClientId(),
                                                     123L, "");
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  ASSERT_EQ(1U, GetAllPages().size());
}

}  // namespace offline_pages
