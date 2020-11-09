// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemVisuals;

namespace {

std::string GetDescriptionIdFromOfflineItemKey(const std::string& id) {
  std::string description_id;
  bool result =
      re2::RE2::FullMatch(id, re2::RE2("\\d+#[^#]+#(.*)"), &description_id);
  EXPECT_TRUE(result);
  return description_id;
}

class ContentIndexTest : public InProcessBrowserTest,
                         public OfflineContentProvider::Observer,
                         public TabStripModelObserver {
 public:
  ContentIndexTest() = default;
  ~ContentIndexTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_->Start());

    ui_test_utils::NavigateToURL(
        browser(), https_server_->GetURL("/content_index/content_index.html"));

    RunScript("RegisterServiceWorker()");

    auto* provider = browser()->profile()->GetContentIndexProvider();
    DCHECK(provider);
    provider_ = static_cast<ContentIndexProviderImpl*>(provider);
    provider_->AddObserver(this);
    browser()->tab_strip_model()->AddObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  // Runs |script| and expects it to complete successfully. |script| must
  // result in a Promise. Returns the resolved contents of the Promise.
  std::string RunScript(const std::string& script) {
    std::string result;
    RunScript(script, &result);
    return result.substr(5);  // Ignore the trailing `ok - `.
  }

  // OfflineContentProvider::Observer implementation:
  void OnItemsAdded(const std::vector<OfflineItem>& items) override {
    ASSERT_EQ(items.size(), 1u);
    offline_items_[GetDescriptionIdFromOfflineItemKey(items[0].id.id)] =
        items[0];
  }

  void OnItemRemoved(const ContentId& id) override {
    offline_items_.erase(GetDescriptionIdFromOfflineItemKey(id.id));
  }

  void OnItemUpdated(
      const OfflineItem& item,
      const base::Optional<offline_items_collection::UpdateDelta>& update_delta)
      override {
    NOTREACHED();
  }

  // TabStripModelObserver implementation:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    if (wait_for_tab_change_)
      std::move(wait_for_tab_change_).Run();
  }

  void SetTabChangeQuitClosure(base::OnceClosure closure) {
    wait_for_tab_change_ = std::move(closure);
  }

  base::Optional<OfflineItem> GetItem(const ContentId& id) {
    base::Optional<OfflineItem> out_item;
    base::RunLoop run_loop;
    provider_->GetItemById(id,
                           base::BindLambdaForTesting(
                               [&](const base::Optional<OfflineItem>& item) {
                                 out_item = item;
                                 run_loop.Quit();
                               }));
    run_loop.Run();
    return out_item;
  }

  std::vector<OfflineItem> GetAllItems() {
    std::vector<OfflineItem> out_items;
    base::RunLoop run_loop;
    provider_->GetAllItems(
        base::BindLambdaForTesting([&](const std::vector<OfflineItem>& items) {
          out_items = items;
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_items;
  }

  std::map<std::string, OfflineItem>& offline_items() { return offline_items_; }
  ContentIndexProviderImpl* provider() { return provider_; }

 private:
  void RunScript(const std::string& script, std::string* result) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        "WrapFunction(async () => " + script + ")", result));
    ASSERT_TRUE(
        base::StartsWith(*result, "ok - ", base::CompareCase::SENSITIVE))
        << "Unexpected result: " << *result;
  }

  std::map<std::string, OfflineItem> offline_items_;
  ContentIndexProviderImpl* provider_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::OnceClosure wait_for_tab_change_;
};

IN_PROC_BROWSER_TEST_F(ContentIndexTest, OfflineItemObserversReceiveEvents) {
  RunScript("AddContent('my-id-1')");
  RunScript("AddContent('my-id-2')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(offline_items().size(), 2u);
  ASSERT_TRUE(offline_items().count("my-id-1"));
  EXPECT_TRUE(offline_items().count("my-id-2"));
  EXPECT_EQ(RunScript("GetIds()"), "my-id-1,my-id-2");
  EXPECT_EQ(GetAllItems().size(), 2u);

  std::string description1 = offline_items().at("my-id-1").description;
  RunScript("AddContent('my-id-1')");     // update
  RunScript("DeleteContent('my-id-2')");  // delete
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(offline_items().size(), 1u);
  ASSERT_TRUE(offline_items().count("my-id-1"));
  EXPECT_FALSE(offline_items().count("my-id-2"));

  // Expect the description to have been updated.
  EXPECT_NE(description1, offline_items().at("my-id-1").description);
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, ContextAPI) {
  EXPECT_TRUE(GetAllItems().empty());

  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.
  {
    const auto& observed_item = offline_items().at("my-id");
    auto items = GetAllItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(observed_item, items[0]);

    auto item = GetItem(observed_item.id);
    ASSERT_TRUE(item);
    EXPECT_EQ(*item, observed_item);
  }

  // Overwrite the existing entry.
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.
  EXPECT_EQ(GetAllItems().size(), 1u);

  // Delete the registration.
  {
    ContentId id = offline_items().at("my-id").id;
    RunScript("DeleteContent('my-id')");
    EXPECT_TRUE(GetAllItems().empty());
    EXPECT_FALSE(GetItem(id));
  }
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, GetVisuals) {
  provider()->SetIconSizesForTesting({{92, 92}});
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  base::RunLoop run_loop;
  SkBitmap icon;
  provider()->GetVisualsForItem(
      offline_items().at("my-id").id,
      OfflineContentProvider::GetVisualsOptions(),
      base::BindLambdaForTesting(
          [&](const ContentId& id,
              std::unique_ptr<OfflineItemVisuals> visuals) {
            ASSERT_EQ(offline_items().at("my-id").id, id);
            ASSERT_TRUE(visuals);
            icon = visuals->icon.AsBitmap();
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(icon.isNull());
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, LaunchUrl) {
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  GURL current_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_TRUE(base::EndsWith(current_url.spec(),
                             "/content_index/content_index.html",
                             base::CompareCase::SENSITIVE));

  provider()->OpenItem(
      offline_items_collection::OpenParams(
          offline_items_collection::LaunchLocation::DOWNLOAD_HOME),
      offline_items().at("my-id").id);

  // Wait for the page to open.
  base::RunLoop run_loop;
  SetTabChangeQuitClosure(run_loop.QuitClosure());
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  current_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_TRUE(base::EndsWith(current_url.spec(),
                             "/content_index/content_index.html?launch",
                             base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, UserDeletedEntryDispatchesEvent) {
  RunScript("AddContent('my-id')");
  base::RunLoop().RunUntilIdle();  // Wait for the provider to get the content.

  provider()->RemoveItem(offline_items().at("my-id").id);
  EXPECT_EQ(RunScript("waitForMessageFromServiceWorker()"), "my-id");
  EXPECT_TRUE(GetAllItems().empty());
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, MetricsCollected) {
  // Inititally there is no content.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(GetAllItems().empty());
    histogram_tester.ExpectUniqueSample("ContentIndex.NumEntriesAvailable", 0,
                                        1);
  }

  // Record that two articles were added.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    {
      base::RunLoop run_loop;
      ukm_recorder.SetOnAddEntryCallback(
          ukm::builders::ContentIndex_Added::kEntryName,
          run_loop.QuitClosure());
      RunScript("AddContent('my-id-1')");
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      ukm_recorder.SetOnAddEntryCallback(
          ukm::builders::ContentIndex_Added::kEntryName,
          run_loop.QuitClosure());
      RunScript("AddContent('my-id-2')");
      run_loop.Run();
    }

    histogram_tester.ExpectBucketCount(
        "ContentIndex.ContentAdded", blink::mojom::ContentCategory::ARTICLE, 2);

    EXPECT_EQ(
        ukm_recorder
            .GetEntriesByName(ukm::builders::ContentIndex_Added::kEntryName)
            .size(),
        2u);
  }

  // Querying the items should record that there are 2 entries available.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(GetAllItems().size(), 2u);
    histogram_tester.ExpectUniqueSample("ContentIndex.NumEntriesAvailable", 2,
                                        1);
  }

  // User deletion will dispatch an event.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::RunLoop run_loop;
    ukm_recorder.SetOnAddEntryCallback(
        ukm::builders::ContentIndex_DeletedByUser::kEntryName,
        run_loop.QuitClosure());
    provider()->RemoveItem(offline_items().at("my-id-1").id);
    EXPECT_EQ(RunScript("waitForMessageFromServiceWorker()"), "my-id-1");
    run_loop.Run();

    histogram_tester.ExpectBucketCount("ContentIndex.ContentDeleteEvent.Find",
                                       blink::ServiceWorkerStatusCode::kOk, 1);
    histogram_tester.ExpectBucketCount("ContentIndex.ContentDeleteEvent.Start",
                                       blink::ServiceWorkerStatusCode::kOk, 1);
    histogram_tester.ExpectBucketCount(
        "ContentIndex.ContentDeleteEvent.Dispatch",
        blink::ServiceWorkerStatusCode::kOk, 1);
    EXPECT_EQ(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::ContentIndex_DeletedByUser::kEntryName)
                  .size(),
              1u);
  }

  // Opening an article is recorded.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    provider()->OpenItem(
        offline_items_collection::OpenParams(
            offline_items_collection::LaunchLocation::DOWNLOAD_HOME),
        offline_items().at("my-id-2").id);

    // Wait for the page to open.
    base::RunLoop run_loop;
    SetTabChangeQuitClosure(run_loop.QuitClosure());
    run_loop.Run();

    histogram_tester.ExpectBucketCount("ContentIndex.ContentOpened",
                                       blink::mojom::ContentCategory::ARTICLE,
                                       1);

    EXPECT_EQ(
        ukm_recorder
            .GetEntriesByName(ukm::builders::ContentIndex_Opened::kEntryName)
            .size(),
        1u);
  }
}

}  // namespace
