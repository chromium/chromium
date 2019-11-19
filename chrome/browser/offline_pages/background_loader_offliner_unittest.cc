// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/background_loader_offliner.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/offline_pages/offliner_helper.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/offline_pages/content/background_loader/background_loader_contents_stub.h"
#include "components/offline_pages/core/background/load_termination_listener.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
char kShortSnapshotDelayForTest[] =
    "short-offline-page-snapshot-delay-for-test";
}  // namespace

namespace offline_pages {

namespace {

using security_state::VisibleSecurityState;

const int64_t kRequestId = 7;
const GURL kHttpUrl("http://www.tunafish.com");
const GURL kHttpsUrl("https://www.yellowtail.com");
const GURL kFileUrl("file://salmon.png");
const ClientId kClientId("async_loading", "88");
const bool kUserRequested = true;
const char kRequestOrigin[] = "abc.xyz";

class TestLoadTerminationListener : public LoadTerminationListener {
 public:
  TestLoadTerminationListener() = default;
  ~TestLoadTerminationListener() override = default;

  void TerminateLoad() { offliner()->TerminateLoadIfInProgress(); }

  Offliner* offliner() { return offliner_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLoadTerminationListener);
};

// Mock OfflinePageModel for testing the SavePage calls
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel() : mock_saving_(false), mock_deleting_(false) {}
  ~MockOfflinePageModel() override {}

  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                content::WebContents* web_contents,
                SavePageCallback callback) override {
    mock_saving_ = true;
    save_page_callback_ = std::move(callback);
    save_page_params_ = save_page_params;
  }

  void CompleteSavingAsArchiveCreationFailed() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(save_page_callback_),
                                  SavePageResult::ARCHIVE_CREATION_FAILED, 0));
  }

  void CompleteSavingAsSuccess() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(save_page_callback_),
                                  SavePageResult::SUCCESS, 123456));
  }

  void CompleteSavingAsAlreadyExists() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(save_page_callback_),
                                  SavePageResult::ALREADY_EXISTS, 123456));
  }

  void DeletePagesWithCriteria(const PageCriteria& criteria,
                               DeletePageCallback callback) override {
    mock_deleting_ = true;
    std::move(callback).Run(DeletePageResult::SUCCESS);
  }

  bool mock_saving() const { return mock_saving_; }
  bool mock_deleting() const { return mock_deleting_; }
  SavePageParams& save_page_params() { return save_page_params_; }

 private:
  bool mock_saving_;
  bool mock_deleting_;
  SavePageCallback save_page_callback_;
  SavePageParams save_page_params_;

  DISALLOW_COPY_AND_ASSIGN(MockOfflinePageModel);
};

}  // namespace

// A BackgroundLoader that we can run tests on.
// Overrides the ResetState so we don't actually try to create any web contents.
// This is a temporary solution to test core BackgroundLoaderOffliner
// functionality until we straighten out assumptions made by RequestCoordinator
// so that the ResetState method is no longer needed.
class TestBackgroundLoaderOffliner : public BackgroundLoaderOffliner {
 public:
  explicit TestBackgroundLoaderOffliner(
      content::BrowserContext* browser_context,
      const OfflinerPolicy* policy,
      OfflinePageModel* offline_page_model,
      std::unique_ptr<LoadTerminationListener> load_termination_listener);
  ~TestBackgroundLoaderOffliner() override;
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(stub_->web_contents());
  }

  content::WebContents* web_contents() { return stub_->web_contents(); }
  background_loader::BackgroundLoaderContents* stub() { return stub_; }

  bool is_loading() { return loader_ && stub_->is_loading(); }

  void set_custom_visible_security_state(
      std::unique_ptr<VisibleSecurityState> visible_security_state) {
    custom_visible_security_state_ = std::move(visible_security_state);
  }
  void set_page_type(content::PageType page_type) { page_type_ = page_type; }

 private:
  // BackgroundLoaderOffliner overrides.
  void ResetLoader() override;
  std::unique_ptr<VisibleSecurityState> GetVisibleSecurityState(
      content::WebContents* web_contents) override;
  content::PageType GetPageType(content::WebContents* web_contents) override;

  background_loader::BackgroundLoaderContentsStub* stub_;
  std::unique_ptr<VisibleSecurityState> custom_visible_security_state_;
  content::PageType page_type_ = content::PageType::PAGE_TYPE_NORMAL;
};

TestBackgroundLoaderOffliner::TestBackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model,
    std::unique_ptr<LoadTerminationListener> load_termination_listener)
    : BackgroundLoaderOffliner(browser_context,
                               policy,
                               offline_page_model,
                               std::move(load_termination_listener)) {}

TestBackgroundLoaderOffliner::~TestBackgroundLoaderOffliner() {}

void TestBackgroundLoaderOffliner::ResetLoader() {
  stub_ = new background_loader::BackgroundLoaderContentsStub(browser_context_);
  loader_.reset(stub_);
  loader_->SetDelegate(this);
}

std::unique_ptr<VisibleSecurityState>
TestBackgroundLoaderOffliner::GetVisibleSecurityState(
    content::WebContents* web_contents) {
  if (custom_visible_security_state_)
    return std::move(custom_visible_security_state_);
  return BackgroundLoaderOffliner::GetVisibleSecurityState(web_contents);
}

content::PageType TestBackgroundLoaderOffliner::GetPageType(
    content::WebContents* web_contents) {
  return page_type_;
}

class BackgroundLoaderOfflinerTest : public testing::Test {
 public:
  BackgroundLoaderOfflinerTest();
  ~BackgroundLoaderOfflinerTest() override;

  void SetUp() override;

  TestBackgroundLoaderOffliner* offliner() const { return offliner_.get(); }
  Offliner::CompletionCallback completion_callback() {
    return base::BindOnce(&BackgroundLoaderOfflinerTest::OnCompletion,
                          base::Unretained(this));
  }
  Offliner::ProgressCallback const progress_callback() {
    return base::BindRepeating(&BackgroundLoaderOfflinerTest::OnProgress,
                               base::Unretained(this));
  }
  Offliner::CancelCallback cancel_callback() {
    return base::BindOnce(&BackgroundLoaderOfflinerTest::OnCancel,
                          base::Unretained(this));
  }
  base::Callback<void(bool)> const can_download_callback() {
    return base::Bind(&BackgroundLoaderOfflinerTest::OnCanDownload,
                      base::Unretained(this));
  }
  Profile* profile() { return &profile_; }
  bool completion_callback_called() { return completion_callback_called_; }
  Offliner::RequestStatus request_status() { return request_status_; }
  bool cancel_callback_called() { return cancel_callback_called_; }
  bool can_download_callback_called() { return can_download_callback_called_; }
  bool can_download() { return can_download_; }
  bool SaveInProgress() const { return model_->mock_saving(); }
  bool DeleteCalled() const { return model_->mock_deleting(); }
  MockOfflinePageModel* model() const { return model_; }
  const base::HistogramTester& histograms() const { return histogram_tester_; }
  int64_t progress() { return progress_; }
  OfflinerPolicy* policy() const { return policy_.get(); }
  TestLoadTerminationListener* load_termination_listener() {
    return load_termination_listener_;
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  void CompleteLoading() {
    // Reset snapshot controller.
    std::unique_ptr<BackgroundSnapshotController> snapshot_controller(
        new BackgroundSnapshotController(base::ThreadTaskRunnerHandle::Get(),
                                         offliner_.get(),
                                         false /* RenovationsEnabled */));
    offliner_->SetBackgroundSnapshotControllerForTest(
        std::move(snapshot_controller));
    // Call complete loading.
    offliner()->DocumentAvailableInMainFrame();
    offliner()->DocumentOnLoadCompletedInMainFrame();
    PumpLoop();
  }

  offline_pages::RequestStats* GetRequestStats() {
    return offliner_->GetRequestStatsForTest();
  }

  std::unique_ptr<VisibleSecurityState> BaseVisibleSecurityState() {
    auto visible_security_state = std::make_unique<VisibleSecurityState>();
    visible_security_state->connection_info_initialized = true;
    visible_security_state->url = kHttpsUrl;
    visible_security_state->certificate =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_2016.pem");
    visible_security_state->cert_status =
        net::CERT_STATUS_SHA1_SIGNATURE_PRESENT;
    return visible_security_state;
  }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::RequestStatus status);
  void OnProgress(const SavePageRequest& request, int64_t bytes);
  void OnCancel(const SavePageRequest& request);
  void OnCanDownload(bool allowed);
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvhte_;
  TestingProfile profile_;
  std::unique_ptr<OfflinerPolicy> policy_;
  TestLoadTerminationListener* load_termination_listener_;
  std::unique_ptr<TestBackgroundLoaderOffliner> offliner_;
  MockOfflinePageModel* model_;
  bool completion_callback_called_;
  bool cancel_callback_called_;
  bool can_download_callback_called_;
  bool can_download_;
  int64_t progress_;
  Offliner::RequestStatus request_status_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoaderOfflinerTest);
};

BackgroundLoaderOfflinerTest::BackgroundLoaderOfflinerTest()
    : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
      load_termination_listener_(nullptr),
      model_(nullptr),
      completion_callback_called_(false),
      cancel_callback_called_(false),
      can_download_callback_called_(false),
      can_download_(false),
      progress_(0LL),
      request_status_(Offliner::RequestStatus::UNKNOWN) {}

BackgroundLoaderOfflinerTest::~BackgroundLoaderOfflinerTest() {}

void BackgroundLoaderOfflinerTest::SetUp() {
  // Set the snapshot controller delay command line switch to short delays.
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(kShortSnapshotDelayForTest);

  std::unique_ptr<TestLoadTerminationListener> listener =
      std::make_unique<TestLoadTerminationListener>();
  load_termination_listener_ = listener.get();
  model_ = new MockOfflinePageModel();
  policy_.reset(new OfflinerPolicy());
  offliner_.reset(new TestBackgroundLoaderOffliner(
      profile(), policy_.get(), model_, std::move(listener)));
}

void BackgroundLoaderOfflinerTest::OnCompletion(
    const SavePageRequest& request,
    Offliner::RequestStatus status) {
  DCHECK(!completion_callback_called_);  // Expect 1 callback per request.
  completion_callback_called_ = true;
  request_status_ = status;
}

void BackgroundLoaderOfflinerTest::OnProgress(const SavePageRequest& request,
                                              int64_t bytes) {
  progress_ = bytes;
}

void BackgroundLoaderOfflinerTest::OnCancel(const SavePageRequest& request) {
  DCHECK(!cancel_callback_called_);
  cancel_callback_called_ = true;
}

void BackgroundLoaderOfflinerTest::OnCanDownload(bool allowed) {
  can_download_callback_called_ = true;
  can_download_ = allowed;
}

TEST_F(BackgroundLoaderOfflinerTest, LoadTerminationListenerSetup) {
  // Verify that back pointer to offliner is set up in the listener.
  Offliner* base_offliner = offliner();
  EXPECT_NE(base_offliner, nullptr);
  EXPECT_EQ(base_offliner, load_termination_listener()->offliner());
}

TEST_F(BackgroundLoaderOfflinerTest,
       LoadAndSaveBlockThirdPartyCookiesForCustomTabs) {
  base::Time creation_time = base::Time::Now();
  ClientId custom_tabs_client_id("custom_tabs", "88");
  SavePageRequest request(kRequestId, kHttpUrl, custom_tabs_client_id,
                          creation_time, kUserRequested);

  profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
}

TEST_F(BackgroundLoaderOfflinerTest,
       LoadAndSaveNetworkPredictionDisabledForCustomTabs) {
  base::Time creation_time = base::Time::Now();
  ClientId custom_tabs_client_id("custom_tabs", "88");
  SavePageRequest request(kRequestId, kHttpUrl, custom_tabs_client_id,
                          creation_time, kUserRequested);

  profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
}

TEST_F(BackgroundLoaderOfflinerTest, LoadAndSaveStartsLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_TRUE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
  EXPECT_FALSE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, BytesReportedWillUpdateProgress) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->OnNetworkBytesChanged(5LL);
  EXPECT_EQ(progress(), 5LL);
  offliner()->OnNetworkBytesChanged(10LL);
  EXPECT_EQ(progress(), 15LL);
}

TEST_F(BackgroundLoaderOfflinerTest, CompleteLoadingInitiatesSave) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_request_origin(kRequestOrigin);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  CompleteLoading();
  PumpLoop();
  // Verify that request origin is propagated.
  EXPECT_EQ(kRequestOrigin, model()->save_page_params().request_origin);
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(SaveInProgress());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, CancelWhenLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->Cancel(cancel_callback());
  PumpLoop();
  offliner()->OnNetworkBytesChanged(15LL);
  EXPECT_TRUE(cancel_callback_called());
  EXPECT_FALSE(completion_callback_called());
  EXPECT_FALSE(offliner()->is_loading());  // Offliner reset.
  EXPECT_EQ(progress(), 0LL);  // network bytes not recorded when not busy.
}

TEST_F(BackgroundLoaderOfflinerTest, CancelWhenLoadTerminated) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  load_termination_listener()->TerminateLoad();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_FALSE(offliner()->is_loading());  // Offliner reset.
  EXPECT_EQ(Offliner::RequestStatus::FOREGROUND_CANCELED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, CancelWhenLoaded) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  CompleteLoading();
  PumpLoop();
  offliner()->Cancel(cancel_callback());
  PumpLoop();

  // Subsequent save callback cause no crash.
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_TRUE(cancel_callback_called());
  EXPECT_TRUE(DeleteCalled());
  EXPECT_FALSE(completion_callback_called());
  EXPECT_FALSE(SaveInProgress());
  EXPECT_FALSE(offliner()->is_loading());  // Offliner reset.
}

TEST_F(BackgroundLoaderOfflinerTest, LoadedButSaveFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVE_FAILED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, ProgressDoesNotUpdateDuringSave) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->OnNetworkBytesChanged(10LL);
  CompleteLoading();
  PumpLoop();
  offliner()->OnNetworkBytesChanged(15LL);
  EXPECT_EQ(progress(), 10LL);
}

TEST_F(BackgroundLoaderOfflinerTest, LoadAndSaveSuccess) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsSuccess();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(can_download_callback_called());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, LoadAndSaveAlreadyExists) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsAlreadyExists();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, ResetsWhenDownloadStarts) {
  base::Time creation_time = base::Time::Now();
  ClientId browser_actions("browser_actions", "123");
  SavePageRequest request(kRequestId, kHttpUrl, browser_actions, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->stub()->CanDownload(kHttpUrl, "foo", can_download_callback());
  PumpLoop();
  EXPECT_TRUE(can_download_callback_called());
  EXPECT_TRUE(can_download());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::DOWNLOAD_THROTTLED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, ResetsWhenDownloadEncountered) {
  base::Time creation_time = base::Time::Now();
  ClientId prefetching("suggested_articles", "123");
  SavePageRequest request(kRequestId, kHttpUrl, prefetching, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->stub()->CanDownload(kHttpUrl, "foo", can_download_callback());
  PumpLoop();
  EXPECT_TRUE(can_download_callback_called());
  EXPECT_FALSE(can_download());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, CanDownloadReturnsIfNoPendingRequest) {
  offliner()->CanDownload(can_download_callback());
  PumpLoop();
  EXPECT_TRUE(can_download_callback_called());
  EXPECT_FALSE(can_download());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnInvalidURL) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kFileUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnRenderCrash) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->RenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED_NO_NEXT, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnRenderKilled) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->RenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED);

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, ReturnsOnWebContentsDestroyed) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  offliner()->WebContentsDestroyed();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnErrorPage) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  // Create handle with net error code.
  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  handle.set_is_error_page(true);
  handle.set_net_error_code(net::Error::ERR_NAME_NOT_RESOLVED);
  offliner()->DidFinishNavigation(&handle);

  histograms().ExpectBucketCount(
      "OfflinePages.Background.LoadingErrorStatusCode.async_loading",
      -105,  // ERR_NAME_NOT_RESOLVED
      1);
  CompleteLoading();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED_NET_ERROR,
            request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnCertificateError) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Sets the certificate status as having been revoked.
  std::unique_ptr<VisibleSecurityState> visible_security_state =
      BaseVisibleSecurityState();
  visible_security_state->cert_status |= net::CERT_STATUS_REVOKED;
  offliner()->set_custom_visible_security_state(
      std::move(visible_security_state));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_FALSE(SaveInProgress());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR,
            request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnRevocationCheckingFailure) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Sets a revocation checking failure certificate error that should not be
  // allowed.
  std::unique_ptr<VisibleSecurityState> visible_security_state =
      BaseVisibleSecurityState();
  visible_security_state->cert_status |=
      net::CERT_STATUS_NO_REVOCATION_MECHANISM;
  offliner()->set_custom_visible_security_state(
      std::move(visible_security_state));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_FALSE(SaveInProgress());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR,
            request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, SucceedsOnHttp) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Sets the URL to HTTP while still setting a major certificate error (should
  // be ignored).
  std::unique_ptr<VisibleSecurityState> visible_security_state =
      BaseVisibleSecurityState();
  visible_security_state->url = kHttpUrl;
  visible_security_state->cert_status |= net::CERT_STATUS_REVOKED;
  offliner()->set_custom_visible_security_state(
      std::move(visible_security_state));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_TRUE(SaveInProgress());
  EXPECT_FALSE(completion_callback_called());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnUnwantedContent) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Sets the page as containing SafeBrowsing unwanted content.
  std::unique_ptr<VisibleSecurityState> visible_security_state =
      BaseVisibleSecurityState();
  visible_security_state->malicious_content_status = security_state::
      MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  offliner()->set_custom_visible_security_state(
      std::move(visible_security_state));
  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_FALSE(SaveInProgress());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED_PAGE_IS_BLOCKED, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnInterstitialPage) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Sets the page as being an interstitial.
  offliner()->set_page_type(content::PageType::PAGE_TYPE_INTERSTITIAL);
  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_FALSE(SaveInProgress());
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADED_PAGE_IS_CHROME_INTERNAL,
            request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, FailsOnInternetDisconnected) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Create handle with net error code.
  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  handle.set_is_error_page(true);
  handle.set_net_error_code(net::Error::ERR_INTERNET_DISCONNECTED);
  offliner()->DidFinishNavigation(&handle);

  CompleteLoading();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED_NET_ERROR,
            request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, DoesNotCrashWithNullResponseHeaders) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  offliner()->DidFinishNavigation(&handle);
}

TEST_F(BackgroundLoaderOfflinerTest, OffliningPreviewsStatusOffHistogram) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  // Set up PreviewsUserData on the handle.
  PreviewsUITabHelper::CreateForWebContents(offliner()->web_contents());
  PreviewsUITabHelper::FromWebContents(offliner()->web_contents())
      ->CreatePreviewsUserDataForNavigationHandle(&handle, 1u)
      ->set_committed_previews_state(
          content::PreviewsTypes::PREVIEWS_NO_TRANSFORM);
  scoped_refptr<net::HttpResponseHeaders> header(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  handle.set_response_headers(header.get());
  // Call DidFinishNavigation with handle.
  offliner()->DidFinishNavigation(&handle);

  histograms().ExpectBucketCount(
      "OfflinePages.Background.OffliningPreviewStatus.async_loading",
      0,  // Previews Disabled
      1);
}

TEST_F(BackgroundLoaderOfflinerTest, OffliningPreviewsStatusOnHistogram) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Called after calling LoadAndSave so we have web_contents to work with.
  content::MockNavigationHandle handle(
      kHttpUrl, offliner()->web_contents()->GetMainFrame());
  handle.set_has_committed(true);
  // Set up PreviewsUserData on the handle.
  PreviewsUITabHelper::CreateForWebContents(offliner()->web_contents());
  PreviewsUITabHelper::FromWebContents(offliner()->web_contents())
      ->CreatePreviewsUserDataForNavigationHandle(&handle, 1u)
      ->set_committed_previews_state(content::PreviewsTypes::NOSCRIPT_ON);
  scoped_refptr<net::HttpResponseHeaders> header(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  handle.set_response_headers(header.get());

  // Call DidFinishNavigation with handle.
  offliner()->DidFinishNavigation(&handle);

  histograms().ExpectBucketCount(
      "OfflinePages.Background.OffliningPreviewStatus.async_loading",
      1,  // Previews Enabled
      1);
}

TEST_F(BackgroundLoaderOfflinerTest, OnlySavesOnceOnMultipleLoads) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  // First load
  CompleteLoading();
  // Second load
  CompleteLoading();
  PumpLoop();
  model()->CompleteSavingAsSuccess();
  PumpLoop();

  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(offliner()->is_loading());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, HandleTimeoutWithLowBarStartedTriesMet) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_started_attempt_count(policy()->GetMaxStartedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  // Guarantees low bar for saving is met.
  offliner()->DocumentAvailableInMainFrame();
  // Timeout
  EXPECT_TRUE(offliner()->HandleTimeout(kRequestId));
  EXPECT_TRUE(SaveInProgress());
  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_EQ(Offliner::RequestStatus::SAVED_ON_LAST_RETRY, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, HandleTimeoutWithLowBarCompletedTriesMet) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_completed_attempt_count(policy()->GetMaxCompletedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  // Guarantees low bar for saving is met.
  offliner()->DocumentAvailableInMainFrame();
  // Timeout
  EXPECT_TRUE(offliner()->HandleTimeout(kRequestId));
  EXPECT_TRUE(SaveInProgress());
  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_EQ(Offliner::RequestStatus::SAVED_ON_LAST_RETRY, request_status());
}

TEST_F(BackgroundLoaderOfflinerTest, HandleTimeoutWithNoLowBarStartedTriesMet) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  request.set_started_attempt_count(policy()->GetMaxStartedTries() - 1);
  // Timeout
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest,
       HandleTimeoutWithNoLowBarCompletedTriesMet) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  request.set_completed_attempt_count(policy()->GetMaxCompletedTries() - 1);
  // Timeout
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, HandleTimeoutWithLowBarNoRetryLimit) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  // Sets lowbar.
  offliner()->DocumentAvailableInMainFrame();
  // Timeout
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(BackgroundLoaderOfflinerTest, SignalCollectionDisabled) {
  // Ensure feature flag for Signal collection is off,
  EXPECT_FALSE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  CompleteLoading();
  PumpLoop();

  // No extra parts should be added if the flag is off.
  content::MHTMLExtraParts* extra_parts =
      content::MHTMLExtraParts::FromWebContents(offliner()->web_contents());
  EXPECT_EQ(extra_parts->size(), 0);
}

TEST_F(BackgroundLoaderOfflinerTest, SignalCollectionEnabled) {
  // Ensure feature flag for signal collection is on.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesLoadSignalCollectingFeature);
  EXPECT_TRUE(IsOfflinePagesLoadSignalCollectingEnabled());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  CompleteLoading();
  PumpLoop();

  // One extra part should be added if the flag is on.
  content::MHTMLExtraParts* extra_parts =
      content::MHTMLExtraParts::FromWebContents(offliner()->web_contents());
  EXPECT_EQ(extra_parts->size(), 1);
}

TEST_F(BackgroundLoaderOfflinerTest, ResourceSignalCollection) {
  // Ensure feature flag for signal collection is on.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesLoadSignalCollectingFeature);
  EXPECT_TRUE(IsOfflinePagesLoadSignalCollectingEnabled());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));

  // Simulate resource requests starting and completing
  offliner()->ObserveResourceLoading(
      ResourceLoadingObserver::ResourceDataType::IMAGE, true);
  offliner()->ObserveResourceLoading(
      ResourceLoadingObserver::ResourceDataType::IMAGE, false);
  offliner()->ObserveResourceLoading(
      ResourceLoadingObserver::ResourceDataType::TEXT_CSS, true);
  offliner()->ObserveResourceLoading(
      ResourceLoadingObserver::ResourceDataType::TEXT_CSS, true);
  offliner()->ObserveResourceLoading(
      ResourceLoadingObserver::ResourceDataType::XHR, true);

  CompleteLoading();
  PumpLoop();

  // One extra part should be added if the flag is on.
  content::MHTMLExtraParts* extra_parts =
      content::MHTMLExtraParts::FromWebContents(offliner()->web_contents());
  EXPECT_EQ(extra_parts->size(), 1);

  offline_pages::RequestStats* stats = GetRequestStats();
  EXPECT_EQ(1,
            stats[ResourceLoadingObserver::ResourceDataType::IMAGE].requested);
  EXPECT_EQ(1,
            stats[ResourceLoadingObserver::ResourceDataType::IMAGE].completed);
  EXPECT_EQ(
      2, stats[ResourceLoadingObserver::ResourceDataType::TEXT_CSS].requested);
  EXPECT_EQ(1, stats[ResourceLoadingObserver::ResourceDataType::XHR].requested);
}

}  // namespace offline_pages
