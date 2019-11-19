// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/logger.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/origin.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentProvider;
using GetVisualsOptions =
    offline_items_collection::OfflineContentProvider::GetVisualsOptions;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemFilter;
using offline_items_collection::OfflineItemProgressUnit;
using offline_items_collection::OfflineItemVisuals;

namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

// Scripts run by this test are defined in
// chrome/test/data/background_fetch/background_fetch.js.

// URL of the test helper page that helps drive these tests.
const char kHelperPage[] = "/background_fetch/background_fetch.html";

// Name of the Background Fetch client as known by the download service.
const char kBackgroundFetchClient[] = "BackgroundFetch";

// Stringified values of request services made to the download service.
const char kResultAccepted[] = "ACCEPTED";

// Title of a Background Fetch started by StartSingleFileDownload().
const char kSingleFileDownloadTitle[] = "Single-file Background Fetch";

// Size of the downloaded resource, used in BackgroundFetch tests.
const int kDownloadedResourceSizeInBytes = 82;

// Incorrect downloadTotal, when it's set too high in the JavaScript file
// loaded by this test.
const int kDownloadTotalBytesTooHigh = 1000;

// Incorrect downloadTotal, when it's set too low in the JavaScript file
// loaded by this test.
const int kDownloadTotalBytesTooLow = 80;

// Number of requests in the fetch() call from the JavaScript file loaded by
// this test.
const int kNumRequestsInFetch = 1;

// Number of icons in the fetch() call from the JavaScript file loaded by this
// test.
const int kNumIconsInFetch = 1;

// Exponential bucket spacing for UKM event data.
const double kUkmEventDataBucketSpacing = 2.0;

// Implementation of a download system logger that provides the ability to wait
// for certain events to happen, notably added and progressing downloads.
class WaitableDownloadLoggerObserver : public download::Logger::Observer {
 public:
  using DownloadAcceptedCallback =
      base::OnceCallback<void(const std::string& guid)>;

  WaitableDownloadLoggerObserver() = default;
  ~WaitableDownloadLoggerObserver() override = default;

  // Sets the |callback| to be invoked when a download has been accepted.
  void set_download_accepted_callback(DownloadAcceptedCallback callback) {
    download_accepted_callback_ = std::move(callback);
  }

  // download::Logger::Observer implementation:
  void OnServiceStatusChanged(const base::Value& service_status) override {}
  void OnServiceDownloadsAvailable(
      const base::Value& service_downloads) override {}
  void OnServiceDownloadChanged(const base::Value& service_download) override {}
  void OnServiceDownloadFailed(const base::Value& service_download) override {}
  void OnServiceRequestMade(const base::Value& service_request) override {
    const std::string& client = service_request.FindKey("client")->GetString();
    const std::string& guid = service_request.FindKey("guid")->GetString();
    const std::string& result = service_request.FindKey("result")->GetString();

    if (client != kBackgroundFetchClient)
      return;  // This event is not targeted to us.

    if (result == kResultAccepted && download_accepted_callback_)
      std::move(download_accepted_callback_).Run(guid);
  }

 private:
  DownloadAcceptedCallback download_accepted_callback_;

  DISALLOW_COPY_AND_ASSIGN(WaitableDownloadLoggerObserver);
};

// Observes the offline item collection's content provider and then invokes the
// associated test callbacks when one has been provided.
class OfflineContentProviderObserver : public OfflineContentProvider::Observer {
 public:
  using ItemsAddedCallback =
      base::OnceCallback<void(const std::vector<OfflineItem>&)>;
  using ItemUpdatedCallback = base::OnceCallback<void(const OfflineItem&)>;
  using FinishedProcessingItemCallback =
      base::OnceCallback<void(const OfflineItem&)>;

  OfflineContentProviderObserver() = default;
  ~OfflineContentProviderObserver() final = default;

  void set_items_added_callback(ItemsAddedCallback callback) {
    items_added_callback_ = std::move(callback);
  }

  void set_finished_processing_item_callback(
      FinishedProcessingItemCallback callback) {
    finished_processing_item_callback_ = std::move(callback);
  }

  void set_delegate(BackgroundFetchDelegateImpl* delegate) {
    delegate_ = delegate;
  }

  void PauseOnNextUpdate() {
    DCHECK(!resume_);
    pause_ = true;
  }

  void ResumeOnNextUpdate() {
    DCHECK(!pause_);
    resume_ = true;
  }

  // OfflineContentProvider::Observer implementation:
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override {
    if (items_added_callback_)
      std::move(items_added_callback_).Run(items);
  }

  void OnItemRemoved(const ContentId& id) override {}
  void OnItemUpdated(
      const OfflineItem& item,
      const base::Optional<offline_items_collection::UpdateDelta>& update_delta)
      override {
    if (item.state != offline_items_collection::OfflineItemState::IN_PROGRESS &&
        item.state != offline_items_collection::OfflineItemState::PENDING &&
        item.state != offline_items_collection::OfflineItemState::PAUSED &&
        finished_processing_item_callback_) {
      std::move(finished_processing_item_callback_).Run(item);
    }

    if (pause_) {
      if (item.state == offline_items_collection::OfflineItemState::PAUSED) {
        Resume(item.id);
        pause_ = false;
      } else {
        delegate_->PauseDownload(item.id);
      }
    }

    if (resume_ &&
        item.state == offline_items_collection::OfflineItemState::PAUSED) {
      Resume(item.id);
      resume_ = false;
    }

    // Check that the progress is always increasing and never resets.
    DCHECK_GE(item.progress.value, latest_item_.progress.value);
    latest_item_ = item;
  }

  const OfflineItem& latest_item() const { return latest_item_; }

 private:
  void Resume(const ContentId& id) {
    delegate_->ResumeDownload(id, false /* has_user_gesture */);
  }

  ItemsAddedCallback items_added_callback_;
  FinishedProcessingItemCallback finished_processing_item_callback_;
  BackgroundFetchDelegateImpl* delegate_ = nullptr;
  bool pause_ = false;
  bool resume_ = false;

  OfflineItem latest_item_;

  DISALLOW_COPY_AND_ASSIGN(OfflineContentProviderObserver);
};

}  // namespace

class BackgroundFetchBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundFetchBrowserTest()
      : offline_content_provider_observer_(
            std::make_unique<OfflineContentProviderObserver>()) {}
  ~BackgroundFetchBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &BackgroundFetchBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    Profile* profile = browser()->profile();

    download_observer_ = std::make_unique<WaitableDownloadLoggerObserver>();

    download_service_ =
        DownloadServiceFactory::GetForKey(profile->GetProfileKey());
    download_service_->GetLogger()->AddObserver(download_observer_.get());

    // Register our observer for the offline items collection.
    OfflineContentAggregatorFactory::GetInstance()
        ->GetForKey(profile->GetProfileKey())
        ->AddObserver(offline_content_provider_observer_.get());

    SetUpBrowser(browser());

    delegate_ = static_cast<BackgroundFetchDelegateImpl*>(
        active_browser_->profile()->GetBackgroundFetchDelegate());
    DCHECK(delegate_);

    offline_content_provider_observer_->set_delegate(delegate_);
  }

  void SetUpBrowser(Browser* browser) {
    active_browser_ = browser;
    // Load the helper page that helps drive these tests.
    ui_test_utils::NavigateToURL(browser, https_server_->GetURL(kHelperPage));

    // Register the Service Worker that's required for Background Fetch. The
    // behaviour without an activated worker is covered by layout tests.
    {
      std::string script_result;
      ASSERT_TRUE(RunScript("RegisterServiceWorker()", &script_result));
      ASSERT_EQ("ok - service worker registered", script_result);
    }

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDownOnMainThread() override {
    OfflineContentAggregatorFactory::GetInstance()
        ->GetForKey(active_browser_->profile()->GetProfileKey())
        ->RemoveObserver(offline_content_provider_observer_.get());

    download_service_->GetLogger()->RemoveObserver(download_observer_.get());
    download_service_ = nullptr;
  }

  // ---------------------------------------------------------------------------
  // Test execution functions.

  // Runs the |script| and waits for one or more items to have been added to the
  // offline items collection. Wrap in ASSERT_NO_FATAL_FAILURE().
  void RunScriptAndWaitForOfflineItems(const std::string& script,
                                       std::vector<OfflineItem>* items) {
    DCHECK(items);

    base::RunLoop run_loop;
    offline_content_provider_observer_->set_items_added_callback(
        base::BindOnce(&BackgroundFetchBrowserTest::DidAddItems,
                       base::Unretained(this), run_loop.QuitClosure(), items));

    std::string result;
    ASSERT_NO_FATAL_FAILURE(RunScript(script, &result));
    ASSERT_EQ("ok", result);

    run_loop.Run();
  }

  // Runs the |script| and waits for a message.
  // Wrap in ASSERT_NO_FATAL_FAILURE().
  void RunScriptAndCheckResultingMessage(const std::string& script,
                                         const std::string& expected_message) {
    std::string result;
    ASSERT_NO_FATAL_FAILURE(RunScript(script, &result));
    ASSERT_EQ(expected_message, result);
  }

  void GetVisualsForOfflineItemSync(
      const ContentId& offline_item_id,
      std::unique_ptr<OfflineItemVisuals>* out_visuals) {
    base::RunLoop run_loop;

    delegate_->GetVisualsForItem(
        offline_item_id, GetVisualsOptions::IconOnly(),
        base::BindOnce(&BackgroundFetchBrowserTest::DidGetVisuals,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_visuals));
    run_loop.Run();
  }

  // ---------------------------------------------------------------------------
  // Helper functions.

  // Runs the |script| in the current tab and writes the output to |*result|.
  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(
        active_browser_->tab_strip_model()
            ->GetActiveWebContents()
            ->GetMainFrame(),
        script, result);
  }

  // Runs the given |function| and asserts that it responds with "ok".
  // Must be wrapped with ASSERT_NO_FATAL_FAILURE().
  void RunScriptFunction(const std::string& function) {
    std::string result;
    ASSERT_TRUE(RunScript(function, &result));
    ASSERT_EQ("ok", result);
  }

  // Intercepts all requests.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    if (request.GetURL().path() == "/background_fetch/upload") {
      DCHECK(!request.content.empty());
      DCHECK(request_body_.empty());
      request_body_ = request.content;

      auto response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }

    if (request.GetURL().query() == "clickevent")
      std::move(click_event_closure_).Run();

    // The default handlers will take care of this request.
    return nullptr;
  }

  // Gets the ideal display size.
  gfx::Size GetIconDisplaySize() {
    gfx::Size out_display_size;
    base::RunLoop run_loop;
    browser()->profile()->GetBackgroundFetchDelegate()->GetIconDisplaySize(
        base::BindOnce(&BackgroundFetchBrowserTest::DidGetIconDisplaySize,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &out_display_size));
    run_loop.Run();
    return out_display_size;
  }

  // Called when we've received the ideal icon display size from
  // BackgroundFetchDelegate.
  void DidGetIconDisplaySize(base::OnceClosure quit_closure,
                             gfx::Size* out_display_size,
                             const gfx::Size& display_size) {
    DCHECK(out_display_size);
    *out_display_size = display_size;
    std::move(quit_closure).Run();
  }

  // Callback for WaitableDownloadLoggerObserver::DownloadAcceptedCallback().
  void DidAcceptDownloadCallback(base::OnceClosure quit_closure,
                                 std::string* out_guid,
                                 const std::string& guid) {
    DCHECK(out_guid);
    *out_guid = guid;

    std::move(quit_closure).Run();
  }

  // Called when the an offline item has been processed.
  void DidFinishProcessingItem(base::OnceClosure quit_closure,
                               OfflineItem* out_item,
                               const OfflineItem& processed_item) {
    DCHECK(out_item);
    *out_item = processed_item;
    std::move(quit_closure).Run();
  }

  void RevokeDownloadPermission() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    DownloadRequestLimiter::TabDownloadState* tab_download_state =
        g_browser_process->download_request_limiter()->GetDownloadState(
            web_contents, true /* create */);
    tab_download_state->set_download_seen();
    tab_download_state->SetDownloadStatusAndNotify(
        url::Origin::Create(web_contents->GetVisibleURL()),
        DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);
  }

  void SetPermission(ContentSettingsType content_type, ContentSetting setting) {
    auto* settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    DCHECK(settings_map);

    ContentSettingsPattern host_pattern =
        ContentSettingsPattern::FromURL(https_server_->base_url());

    settings_map->SetContentSettingCustomScope(
        host_pattern, host_pattern, content_type,
        std::string() /* resource_identifier */, setting);
  }

  void DidUpdateItem(base::OnceClosure quit_closure,
                     OfflineItem* out_item,
                     const OfflineItem& item) {
    *out_item = item;
    std::move(quit_closure).Run();
  }

 protected:
  BackgroundFetchDelegateImpl* delegate_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  base::OnceClosure click_event_closure_;

  std::unique_ptr<WaitableDownloadLoggerObserver> download_observer_;
  std::unique_ptr<OfflineContentProviderObserver>
      offline_content_provider_observer_;
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
  std::string request_body_;

 private:
  // Callback for RunScriptAndWaitForOfflineItems(), called when the |items|
  // have been added to the offline items collection.
  void DidAddItems(base::OnceClosure quit_closure,
                   std::vector<OfflineItem>* out_items,
                   const std::vector<OfflineItem>& items) {
    *out_items = items;
    std::move(quit_closure).Run();
  }

  void DidGetVisuals(base::OnceClosure quit_closure,
                     std::unique_ptr<OfflineItemVisuals>* out_visuals,
                     const ContentId& offline_item_id,
                     std::unique_ptr<OfflineItemVisuals> visuals) {
    *out_visuals = std::move(visuals);
    std::move(quit_closure).Run();
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  Browser* active_browser_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, DownloadService_Acceptance) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // that request to be scheduled with the Download Service.

  std::string guid;
  {
    base::RunLoop run_loop;
    download_observer_->set_download_accepted_callback(
        base::BindOnce(&BackgroundFetchBrowserTest::DidAcceptDownloadCallback,
                       base::Unretained(this), run_loop.QuitClosure(), &guid));

    ASSERT_NO_FATAL_FAILURE(RunScriptFunction("StartSingleFileDownload()"));
    run_loop.Run();
  }

  EXPECT_FALSE(guid.empty());
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       RecordBackgroundFetchUkmEvent) {
  // Start a Background Fetch for a single to-be-downloaded file and  test that
  // the expected UKM data for the BackgroundFetch UKM event has been recorded.

  ASSERT_NO_FATAL_FAILURE(
      RunScriptFunction("StartSingleFileDownloadWithCorrectDownloadTotal()"));

  std::vector<const ukm::mojom::UkmEntry*> entries =
      test_ukm_recorder_->GetEntriesByName(
          ukm::builders::BackgroundFetch::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kHasTitleName, 1);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kNumIconsName, kNumIconsInFetch);
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kDownloadTotalName,
      ukm::GetExponentialBucketMin(kDownloadedResourceSizeInBytes,
                                   kUkmEventDataBucketSpacing));
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kNumRequestsInFetchName,
      ukm::GetExponentialBucketMin(kNumRequestsInFetch,
                                   kUkmEventDataBucketSpacing));
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kDeniedDueToPermissionsName, 0);
  // There is currently no desktop UI for BackgroundFetch, hence the icon
  // display size is set to 0,0. Once that's no longer the case, this ASSERT
  // will start failing and the unit test will have to be updated.
  ASSERT_TRUE(GetIconDisplaySize().IsEmpty());
  test_ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::BackgroundFetch::kRatioOfIdealToChosenIconSizeName,
      -1);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       OfflineItemCollection_SingleFileMetadata) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // the fetch to be registered with the offline items collection. We then
  // verify that all the appropriate values have been set.

  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartSingleFileDownload()", &items));
  ASSERT_EQ(items.size(), 1u);

  const OfflineItem& offline_item = items[0];

  // Verify that the appropriate data is being set.
  EXPECT_EQ(offline_item.title, kSingleFileDownloadTitle);
  EXPECT_EQ(offline_item.filter, OfflineItemFilter::FILTER_OTHER);
  EXPECT_TRUE(offline_item.is_transient);
  EXPECT_TRUE(offline_item.is_resumable);
  EXPECT_FALSE(offline_item.is_suggested);
  EXPECT_FALSE(offline_item.is_off_the_record);

  // When downloadTotal isn't specified, we report progress by parts.
  EXPECT_EQ(offline_item.progress.value, 0);
  EXPECT_EQ(offline_item.progress.max.value(), 1);
  EXPECT_EQ(offline_item.progress.unit, OfflineItemProgressUnit::PERCENTAGE);

  // Change-detector tests for values we might want to provide or change.
  EXPECT_TRUE(offline_item.description.empty());
  EXPECT_TRUE(offline_item.page_url.is_empty());
  EXPECT_FALSE(offline_item.is_off_the_record);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       OfflineItemCollection_VerifyIconReceived) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // the fetch to be registered with the offline items collection. We then
  // verify that the expected icon is associated with the newly added offline
  // item.

  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartSingleFileDownload()", &items));
  ASSERT_EQ(items.size(), 1u);

  const OfflineItem& offline_item = items[0];

  // Verify that the appropriate data is being set.
  EXPECT_EQ(offline_item.progress.value, 0);
  EXPECT_EQ(offline_item.progress.max.value(), 1);
  EXPECT_EQ(offline_item.progress.unit, OfflineItemProgressUnit::PERCENTAGE);

  // Get visuals associated with the newly added offline item.
  std::unique_ptr<OfflineItemVisuals> out_visuals;
  GetVisualsForOfflineItemSync(offline_item.id, &out_visuals);
#if defined(OS_ANDROID)
  EXPECT_FALSE(out_visuals->icon.IsEmpty());
  EXPECT_EQ(out_visuals->icon.Size().width(), 100);
  EXPECT_EQ(out_visuals->icon.Size().height(), 100);
#else
  EXPECT_TRUE(out_visuals->icon.IsEmpty());
#endif
}

IN_PROC_BROWSER_TEST_F(
    BackgroundFetchBrowserTest,
    OfflineItemCollection_VerifyResourceDownloadedWhenDownloadTotalLargerThanActualSize) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // the fetch to be registered with the offline items collection.
  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(RunScriptAndWaitForOfflineItems(
      "StartSingleFileDownloadWithBiggerThanActualDownloadTotal()", &items));
  ASSERT_EQ(items.size(), 1u);

  OfflineItem offline_item = items[0];

  // Verify that the appropriate data is being set when we start downloading.
  EXPECT_EQ(offline_item.progress.value, 0);
  EXPECT_EQ(offline_item.progress.max.value(), kDownloadTotalBytesTooHigh);
  EXPECT_EQ(offline_item.progress.unit, OfflineItemProgressUnit::PERCENTAGE);

  // Wait for the download to be completed.
  {
    base::RunLoop run_loop;
    offline_content_provider_observer_->set_finished_processing_item_callback(
        base::BindOnce(&BackgroundFetchBrowserTest::DidFinishProcessingItem,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &offline_item));
    run_loop.Run();
  }

  // Download total is too high; check that we're still reporting by size,
  // but have set the max value of the progress bar to the actual download size.
  EXPECT_EQ(offline_item.state,
            offline_items_collection::OfflineItemState::COMPLETE);
  EXPECT_EQ(offline_item.progress.max.value(), offline_item.progress.value);
  EXPECT_EQ(offline_item.progress.max.value(), kDownloadedResourceSizeInBytes);
}

IN_PROC_BROWSER_TEST_F(
    BackgroundFetchBrowserTest,
    OfflineItemCollection_VerifyResourceDownloadedWhenDownloadTotalSmallerThanActualSize) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // the fetch to be registered with the offline items collection.
  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(RunScriptAndWaitForOfflineItems(
      "StartSingleFileDownloadWithSmallerThanActualDownloadTotal()", &items));
  ASSERT_EQ(items.size(), 1u);

  OfflineItem offline_item = items[0];

  // Verify that the appropriate data is being set when we start downloading.
  EXPECT_EQ(offline_item.progress.value, 0);
  EXPECT_EQ(offline_item.progress.max.value(), kDownloadTotalBytesTooLow);
  EXPECT_EQ(offline_item.progress.unit, OfflineItemProgressUnit::PERCENTAGE);

  // Wait for the offline_item to be processed.
  {
    base::RunLoop run_loop;
    offline_content_provider_observer_->set_finished_processing_item_callback(
        base::BindOnce(&BackgroundFetchBrowserTest::DidFinishProcessingItem,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &offline_item));
    run_loop.Run();
  }

  // Download total is too low; check that we cancel the download when the
  // bytes downloaded exceed downloadTotal.
  EXPECT_EQ(offline_item.state,
            offline_items_collection::OfflineItemState::CANCELLED);
}

IN_PROC_BROWSER_TEST_F(
    BackgroundFetchBrowserTest,
    OfflineItemCollection_VerifyResourceDownloadedWhenCorrectDownloadTotalSpecified) {
  // Starts a Background Fetch for a single to-be-downloaded file and waits for
  // the fetch to be registered with the offline items collection.

  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(RunScriptAndWaitForOfflineItems(
      "StartSingleFileDownloadWithCorrectDownloadTotal()", &items));
  ASSERT_EQ(items.size(), 1u);

  const OfflineItem& offline_item = items[0];

  // Verify that the appropriate data is being set when downloadTotal is
  // correctly set.
  EXPECT_EQ(offline_item.progress.value, 0);
  EXPECT_EQ(offline_item.progress.max.value(), kDownloadedResourceSizeInBytes);
  EXPECT_EQ(offline_item.progress.unit, OfflineItemProgressUnit::PERCENTAGE);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       OfflineItemCollection_IncognitoPropagated) {
  // Starts a fetch from an incognito profile, and makes sure that the
  // OfflineItem has the appropriate fields set.
  SetUpBrowser(CreateIncognitoBrowser());

  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartSingleFileDownload()", &items));
  ASSERT_EQ(items.size(), 1u);
  ASSERT_TRUE(items[0].is_off_the_record);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchesRunToCompletionAndUpdateTitle_Fetched) {
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchTillCompletion()", "backgroundfetchsuccess"));
  EXPECT_EQ(offline_content_provider_observer_->latest_item().state,
            offline_items_collection::OfflineItemState::COMPLETE);

  base::RunLoop().RunUntilIdle();  // Give `updateUI` a chance to propagate.
  EXPECT_TRUE(
      base::StartsWith(offline_content_provider_observer_->latest_item().title,
                       "New Fetched Title!", base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchesRunToCompletionAndUpdateTitle_Failed) {
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchTillCompletionWithMissingResource()", "backgroundfetchfail"));
  EXPECT_EQ(offline_content_provider_observer_->latest_item().state,
            offline_items_collection::OfflineItemState::COMPLETE);

  base::RunLoop().RunUntilIdle();  // Give `updateUI` a chance to propagate.
  EXPECT_TRUE(
      base::StartsWith(offline_content_provider_observer_->latest_item().title,
                       "New Failed Title!", base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchesRunToCompletion_Upload) {
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchTillCompletionWithUpload()", "backgroundfetchsuccess"));

  EXPECT_EQ(request_body_, "upload!");
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, ClickEventIsDispatched) {
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchTillCompletion()", "backgroundfetchsuccess"));
  EXPECT_EQ(offline_content_provider_observer_->latest_item().state,
            offline_items_collection::OfflineItemState::COMPLETE);

  base::RunLoop().RunUntilIdle();  // Give updates a chance to propagate.

  ASSERT_EQ(delegate_->job_details_map_.size(), 1u);
  auto& job_details = delegate_->job_details_map_.begin()->second;
  EXPECT_EQ(job_details.job_state,
            BackgroundFetchDelegateImpl::JobDetails::State::kJobComplete);

  // Simulate notification click.
  delegate_->OpenItem(offline_items_collection::LaunchLocation::NOTIFICATION,
                      job_details.offline_item.id);

  // Job Details should be deleted at this point.
  EXPECT_TRUE(delegate_->job_details_map_.empty());

  // Wait for click event.
  {
    base::RunLoop run_loop;
    click_event_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, AbortFromUI) {
  std::vector<OfflineItem> items;
  // Creates a registration with more than one request.
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartFetchWithMultipleFiles()", &items));
  ASSERT_EQ(items.size(), 1u);

  // Simulate an abort from the UI.
  delegate_->CancelDownload(items[0].id);

  // Wait for an abort event to be dispatched. This is safe because there are
  // more requests to process than the scheduler will handle, and every request
  // needs to contact the delegate. Since the abort originates from the
  // delegate, this fetch will be aborted before the fetch completes.
  // Pass in a no-op function since we only want to wait for a message at this
  // point.
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "(() => {})()", "backgroundfetchabort"));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, FetchCanBePausedAndResumed) {
  offline_content_provider_observer_->PauseOnNextUpdate();
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchTillCompletion()", "backgroundfetchsuccess"));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchRejectedWithoutPermission) {
  RevokeDownloadPermission();
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "RunFetchAnExpectAnException()",
      "This origin does not have permission to start a fetch."));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, FetchFromServiceWorker) {
  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  DCHECK(settings_map);

  // Give the needed permissions.
  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS,
                CONTENT_SETTING_ALLOW);

  // The fetch should succeed.
  offline_content_provider_observer_->ResumeOnNextUpdate();
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "StartFetchFromServiceWorker()", "backgroundfetchsuccess"));

  // Revoke Automatic Downloads permission.
  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS,
                CONTENT_SETTING_BLOCK);

  // This should fail without the Automatic Downloads permission.
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "StartFetchFromServiceWorker()", "permissionerror"));
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchFromServiceWorkerWithAsk) {
  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  DCHECK(settings_map);

  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS, CONTENT_SETTING_ASK);

  // The fetch starts in a paused state.
  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(RunScriptAndWaitForOfflineItems(
      "StartFetchFromServiceWorkerNoWait()", &items));
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].state,
            offline_items_collection::OfflineItemState::PAUSED);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchFromChildFrameWithPermissions) {
  // Give the needed permissions. The fetch should still start in a paused
  // state.
  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS,
                CONTENT_SETTING_ALLOW);

  // The fetch starts in a paused state.
  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartFetchFromIframeNoWait()", &items));
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].state,
            offline_items_collection::OfflineItemState::PAUSED);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest, FetchFromChildFrameWithAsk) {
  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS, CONTENT_SETTING_ASK);

  // The fetch starts in a paused state.
  std::vector<OfflineItem> items;
  ASSERT_NO_FATAL_FAILURE(
      RunScriptAndWaitForOfflineItems("StartFetchFromIframeNoWait()", &items));
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].state,
            offline_items_collection::OfflineItemState::PAUSED);
}

IN_PROC_BROWSER_TEST_F(BackgroundFetchBrowserTest,
                       FetchFromChildFrameWithMissingPermissions) {
  SetPermission(ContentSettingsType::AUTOMATIC_DOWNLOADS,
                CONTENT_SETTING_BLOCK);
  ASSERT_NO_FATAL_FAILURE(RunScriptAndCheckResultingMessage(
      "StartFetchFromIframe()", "permissionerror"));
}
