// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/background_fetch/job_details.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/background_service/features.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemState;

namespace {

const char kHelperPage[] = "/background_fetch/background_fetch.html";

class TestOfflineContentProviderObserver
    : public OfflineContentProvider::Observer {
 public:
  TestOfflineContentProviderObserver() = default;
  ~TestOfflineContentProviderObserver() override = default;

  void set_delegate(BackgroundFetchDelegateImpl* delegate) {
    delegate_ = delegate;
  }

  void WaitForState(OfflineItemState state) {
    if (latest_item_.state == state ||
        latest_item_.state == OfflineItemState::COMPLETE ||
        latest_item_.state == OfflineItemState::CANCELLED ||
        latest_item_.state == OfflineItemState::FAILED) {
      return;
    }
    target_state_ = state;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override {
    for (const auto& item : items) {
      if (item.id.name_space == "background_fetch") {
        latest_item_ = item;
        if (delegate_ && latest_item_.state != OfflineItemState::PAUSED) {
          delegate_->PauseDownload(latest_item_.id);
        }
        CheckState();
      }
    }
  }

  void OnItemRemoved(const ContentId& id) override {}

  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<offline_items_collection::UpdateDelta>&
                         update_delta) override {
    if (item.id.name_space == "background_fetch") {
      latest_item_ = item;
      CheckState();
    }
  }

  void OnContentProviderGoingDown() override {}

  const OfflineItem& latest_item() const { return latest_item_; }

 private:
  void CheckState() {
    if (run_loop_ && target_state_.has_value()) {
      if (latest_item_.state == target_state_.value() ||
          latest_item_.state == OfflineItemState::COMPLETE ||
          latest_item_.state == OfflineItemState::CANCELLED ||
          latest_item_.state == OfflineItemState::FAILED) {
        run_loop_->Quit();
      }
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<OfflineItemState> target_state_;
  OfflineItem latest_item_;
  raw_ptr<BackgroundFetchDelegateImpl> delegate_ = nullptr;
};

}  // namespace

class BackgroundFetchAndroidBrowserTest : public AndroidBrowserTest {
 public:
  BackgroundFetchAndroidBrowserTest() = default;
  ~BackgroundFetchAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&BackgroundFetchAndroidBrowserTest::HandleRequest,
                            base::Unretained(this)));
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    OfflineContentAggregatorFactory::GetInstance()
        ->GetForKey(GetProfile()->GetProfileKey())
        ->AddObserver(&offline_observer_);

    delegate_ = static_cast<BackgroundFetchDelegateImpl*>(
        GetProfile()->GetBackgroundFetchDelegate());

    // Load the helper page.
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    ASSERT_TRUE(content::NavigateToURL(web_contents,
                                       https_server_->GetURL(kHelperPage)));

    // Register the Service Worker if needed.
    bool sw_registered =
        RunScript(
            "navigator.serviceWorker.getRegistration().then(reg => !!reg)")
            .ExtractBool();
    if (!sw_registered) {
      ASSERT_EQ("ok - service worker registered",
                RunScript("RegisterServiceWorker()"));
    }
  }

  void TearDownOnMainThread() override {
    OfflineContentAggregatorFactory::GetInstance()
        ->GetForKey(GetProfile()->GetProfileKey())
        ->RemoveObserver(&offline_observer_);
  }

 protected:
  Profile* GetProfile() {
    return Profile::FromBrowserContext(
        chrome_test_utils::GetActiveWebContents(this)->GetBrowserContext());
  }

  content::EvalJsResult RunScript(const std::string& script) {
    return content::EvalJs(
        chrome_test_utils::GetActiveWebContents(this)->GetPrimaryMainFrame(),
        script);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() == "/background_fetch/upload") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }
    if (request.GetURL().GetPath() == "/hung") {
      return std::make_unique<net::test_server::HungResponse>();
    }
    return nullptr;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  TestOfflineContentProviderObserver offline_observer_;
  raw_ptr<BackgroundFetchDelegateImpl> delegate_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BackgroundFetchAndroidBrowserTest,
                       PRE_FetchCanBePausedRestartedAndResumed) {
  // Pass the delegate so it can be paused as soon as it's added.
  offline_observer_.set_delegate(delegate_);

  // Start the background fetch.
  std::string script =
      "navigator.serviceWorker.ready.then(reg => { "
      "  return reg.backgroundFetch.fetch('bg-fetch-id', '/hung');"
      "}).then(() => 'ok');";
  ASSERT_EQ("ok", RunScript(script));

  // Wait until the offline item is in a paused state.
  offline_observer_.WaitForState(OfflineItemState::PAUSED);

  // Give some time for DB to flush.
  content::RunAllTasksUntilIdle();
}

// Verifies that a Background Fetch which was paused in a previous browser
// session (setup in PRE_FetchCanBePausedRestartedAndResumed) is correctly
// loaded upon browser restart. It then resumes the fetch and verifies it
// completes successfully.
IN_PROC_BROWSER_TEST_F(BackgroundFetchAndroidBrowserTest,
                       FetchCanBePausedRestartedAndResumed) {
  // Ensure the BackgroundFetchContext is initialized.
  GetProfile()->GetDefaultStoragePartition();

  // First, verify the items are loaded by the aggregator.
  OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetInstance()->GetForKey(
          GetProfile()->GetProfileKey());

  std::vector<OfflineItem> items;
  {
    base::RunLoop run_loop;
    aggregator->GetAllItems(base::BindOnce(
        [](base::OnceClosure quit_closure, std::vector<OfflineItem>* out_items,
           const std::vector<OfflineItem>& items) {
          *out_items = items;
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &items));
    run_loop.Run();
  }

  bool found = false;
  OfflineItem bg_fetch_item;
  for (const auto& item : items) {
    if (item.id.name_space == "background_fetch") {
      found = true;
      bg_fetch_item = item;
      break;
    }
  }

  // Wait for the item to be added (when not found) and in the PAUSED state.
  if (!found || bg_fetch_item.state != OfflineItemState::PAUSED) {
    offline_observer_.WaitForState(OfflineItemState::PAUSED);
    bg_fetch_item = offline_observer_.latest_item();
  }

  // Resume the download.
  if (bg_fetch_item.state == OfflineItemState::PAUSED) {
    delegate_->ResumeDownload(bg_fetch_item.id);
  }

  // Wait until it completes or fails.
  if (offline_observer_.latest_item().state != OfflineItemState::COMPLETE &&
      offline_observer_.latest_item().state != OfflineItemState::CANCELLED &&
      offline_observer_.latest_item().state != OfflineItemState::FAILED) {
    offline_observer_.WaitForState(OfflineItemState::COMPLETE);
  }
}
