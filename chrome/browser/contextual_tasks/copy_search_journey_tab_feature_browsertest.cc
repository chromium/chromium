// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/copy_search_journey_tab_feature.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

class ClipboardObserverWaiter : public content::WebContentsObserver {
 public:
  explicit ClipboardObserverWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override {
    did_copy_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (!did_copy_) {
      run_loop_.Run();
    }
  }

 private:
  bool did_copy_ = false;
  base::RunLoop run_loop_;
};

}  // namespace

class CopySearchJourneyTabFeatureBrowserTest : public InProcessBrowserTest {
 public:
  CopySearchJourneyTabFeatureBrowserTest() {
    feature_list_.InitAndEnableFeature(kCopyTextJourneys);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service());

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              const std::string_view host = params->url_request.url.host();
              if (host.find("google.com") != std::string_view::npos ||
                  host.find("example.com") != std::string_view::npos) {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                    "<html><body>test</body></html>", params->client.get());
                return true;
              }
              return false;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void CopyTextToClipboard(content::WebContents* web_contents,
                           std::string_view copied_text) {
    ClipboardObserverWaiter waiter(web_contents);
    content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
    frame->GetView()->Focus();
    ASSERT_TRUE(content::ExecJs(frame, content::JsReplace(
                                           R"(
      (function() {
        const textarea = document.createElement("textarea");
        textarea.value = $1;
        document.body.append(textarea);
        textarea.select();
        document.execCommand("copy");
        document.body.removeChild(textarea);
      })();
    )",
                                           copied_text)));
    waiter.Wait();
  }

  TemplateURLService* template_url_service() {
    return TemplateURLServiceFactory::GetForProfile(GetProfile());
  }

  CopySearchJourneyTracker* tracker() {
    return CopySearchJourneyTrackerFactory::GetForProfile(GetProfile());
  }

  TabListInterface* tab_list() { return TabListInterface::From(browser()); }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(CopySearchJourneyTabFeatureBrowserTest,
                       RecordsCopyAndCorrelatesCrossTabSearchNavigation) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));

  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  ASSERT_NE(CopySearchJourneyTabFeature::From(source_tab), nullptr);

  content::WebContents* source_contents = source_tab->GetContents();
  const SessionID source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_contents);
  const int source_nav_entry_id =
      source_contents->GetController().GetLastCommittedEntry()->GetUniqueID();

  CopyTextToClipboard(source_contents, "quantum entanglement physics");
  EXPECT_EQ(tracker()->GetRingBufferSizeForTesting(), 1u);
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());

  const GURL search_url =
      template_url_service()->GenerateSearchURLForDefaultSearchProvider(
          u"quantum entanglement physics");
  ASSERT_TRUE(search_url.is_valid());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), search_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* search_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(search_tab);
  content::WebContents* search_contents = search_tab->GetContents();
  ASSERT_NE(search_contents, source_contents);
  const SessionID search_tab_id =
      sessions::SessionTabHelper::IdForTab(search_contents);

  const auto& journeys = tracker()->GetActiveJourneysForTesting();
  ASSERT_EQ(journeys.size(), 1u);
  auto it = journeys.find(source_tab_id);
  ASSERT_NE(it, journeys.end());
  EXPECT_EQ(it->second.copy_record.source_tab_id, source_tab_id);
  EXPECT_EQ(it->second.copy_record.source_nav_entry_id, source_nav_entry_id);
  EXPECT_EQ(it->second.search_tab_id, search_tab_id);
}

IN_PROC_BROWSER_TEST_F(CopySearchJourneyTabFeatureBrowserTest,
                       IgnoresSameTabSearchNavigation) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));

  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  CopyTextToClipboard(source_tab->GetContents(),
                      "quantum entanglement physics");
  EXPECT_EQ(tracker()->GetRingBufferSizeForTesting(), 1u);

  const GURL search_url =
      template_url_service()->GenerateSearchURLForDefaultSearchProvider(
          u"quantum entanglement physics");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(CopySearchJourneyTabFeatureBrowserTest,
                       IgnoresNonSearchNavigation) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));

  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  CopyTextToClipboard(source_tab->GetContents(),
                      "quantum entanglement physics");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("https://example.com/search?q=quantum+entanglement+physics"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(CopySearchJourneyTabFeatureBrowserTest,
                       ClearsStateWhenTabClosed) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));

  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  CopyTextToClipboard(source_tab->GetContents(),
                      "quantum entanglement physics");

  const GURL search_url =
      template_url_service()->GenerateSearchURLForDefaultSearchProvider(
          u"quantum entanglement physics");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), search_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);

  tabs::TabInterface* search_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(search_tab);
  tab_list()->CloseTab(search_tab->GetHandle());
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

class CopySearchJourneyTabFeatureDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  CopySearchJourneyTabFeatureDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kCopyTextJourneys);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CopySearchJourneyTabFeatureDisabledBrowserTest,
                       NotCreatedWhenFeatureDisabled) {
  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_TRUE(tab);
  EXPECT_EQ(CopySearchJourneyTabFeature::From(tab), nullptr);
}

}  // namespace contextual_tasks
