// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace {
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/data_use_measurement");
const char kImagePrefix[] = "/image";

class TestInfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  explicit TestInfoBarObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}
  ~TestInfoBarObserver() override {}

  void OnInfoBarAdded(infobars::InfoBar* infobar) override {}
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    run_loop_->QuitWhenIdle();
  }
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override {}
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
    NOTREACHED();
  }

 private:
  base::RunLoop* run_loop_;
};

}  // namespace

class PageLoadCappingBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadCappingBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PageLoadCappingBrowserTest() override {}

  void PostToSelf() {
    EXPECT_FALSE(waiting_for_infobar_event_ || waiting_for_request_);
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForRequest() {
    EXPECT_FALSE(waiting_for_infobar_event_ || waiting_for_request_);
    waiting_for_request_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void WaitForInfoBarRemoved() {
    EXPECT_FALSE(waiting_for_infobar_event_ || waiting_for_request_);
    waiting_for_infobar_event_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    TestInfoBarObserver test_observer(run_loop_.get());
    InfoBarService::FromWebContents(contents())->AddObserver(&test_observer);
    run_loop_->Run();
    InfoBarService::FromWebContents(contents())->RemoveObserver(&test_observer);
    waiting_for_infobar_event_ = false;
    run_loop_.reset();
  }

  GURL GetURL(const std::string& url_string) {
    return https_test_server_.GetURL(url_string);
  }

  void NavigateToHeavyPage() { NavigateToHeavyPageAnchor(std::string()); }

  void NavigateToHeavyPageAnchor(const std::string& anchor) {
    NavigateToHeavyPageAnchorInBrowser(browser(), anchor);
  }

  void NavigateToHeavyPageAnchorInBrowser(Browser* browser,
                                          const std::string& anchor) {
    ui_test_utils::NavigateToURL(
        browser, GetURL(std::string("/page_capping.html").append(anchor)));
  }

  size_t images_attempted() const { return images_attempted_; }

  content::WebContents* contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  size_t InfoBarCount() const {
    return InfoBarService::FromWebContents(contents())->infobar_count();
  }

  void ClickInfoBarLink() {
    InfoBarService::FromWebContents(contents())
        ->infobar_at(0)
        ->delegate()
        ->AsConfirmInfoBarDelegate()
        ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  }

  void EnableDataSaver(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kDataSaverEnabled,
                                                 enabled);
    base::RunLoop().RunUntilIdle();
  }

 private:
  void SetUp() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial("TrialName1", "GroupName1");
    std::map<std::string, std::string> feature_parameters = {
        {"PageCapMiB", "0"},
        {"PageFuzzingKiB", "0"},
        {"OptOutStoreDisabled", "true"},
        {"InfoBarTimeoutInMilliseconds", "500000"}};
    ChangeParams(&feature_parameters);

    base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
        "TrialName1", "GroupName1", feature_parameters);

    feature_list->RegisterFieldTrialOverride(
        data_use_measurement::page_load_capping::features::kDetectingHeavyPages
            .name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    https_test_server_.RegisterRequestHandler(base::BindRepeating(
        &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
    https_test_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_test_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  virtual void ChangeParams(std::map<std::string, std::string>* params) {}

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Check if this matches the image requests from the test suite.
    if (!StartsWith(request.relative_url, kImagePrefix,
                    base::CompareCase::SENSITIVE)) {
      return nullptr;
    }
    // This request should match "/image.*" for this test suite.
    images_attempted_++;

    // Return a 404. This is expected in the test, but embedded test server will
    // create warnings when serving its own 404 responses.
    std::unique_ptr<net::test_server::BasicHttpResponse> not_found_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    not_found_response->set_code(net::HTTP_NOT_FOUND);
    if (waiting_for_request_) {
      run_loop_->QuitWhenIdle();
      waiting_for_request_ = false;
    }
    return not_found_response;
  }

  net::EmbeddedTestServer https_test_server_;
  size_t images_attempted_ = 0u;
  bool waiting_for_request_ = false;
  bool waiting_for_infobar_event_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingBlocksLoads) {
  // Tests that subresource loading can be blocked from the browser process.

  EnableDataSaver(true);
  // Load a mostly empty page.
  NavigateToHeavyPage();
  // Pause subresource loading.
  ClickInfoBarLink();

  // Adds images to the page. They should not be allowed to load.
  // Running this 20 times makes 20 round trips to the renderer, making it very
  // likely the earliest request would have made it to the network by the time
  // all of the calls have been made.
  for (size_t i = 0; i < 20; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = '")
            .append(kImagePrefix)
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents(), create_image_script));
  }

  // No images should be loaded as subresource loading was paused.
  EXPECT_EQ(0u, images_attempted());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlocksLoadsAndResume) {
  // Tests that after triggerring subresource pausing, resuming allows deferred
  // requests to be initiated.

  EnableDataSaver(true);
  // Load a mostly empty page.
  NavigateToHeavyPage();
  // Pause subresource loading.
  ClickInfoBarLink();

  // Adds an image to the page. It should not be allowed to load at first.
  // PageLoadCappingBlocksLoads tests that it is not loaded more robustly
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents(), create_image_script));

  // Previous image should be allowed to load now.
  ClickInfoBarLink();

  // An image should be fetched because subresource loading was paused then
  // resumed.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingAllowLoads) {
  // Tests that the image request loads normally when the page has not been
  // paused.

  EnableDataSaver(true);
  // Load a mostly empty page.
  NavigateToHeavyPage();

  // Adds an image to the page. It should be allowed to load.
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents(), create_image_script));

  // An image should be fetched because subresource loading was never paused.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlockNewFrameLoad) {
  // Tests that the image request loads normally when the page has not been
  // paused.

  EnableDataSaver(true);
  // Load a mostly empty page.
  NavigateToHeavyPage();
  // Pause subresource loading.
  ClickInfoBarLink();
  content::TestNavigationObserver load_observer(contents());

  // Adds an image to the page. It should be allowed to load.
  std::string create_iframe_script = std::string(
      "var iframe = document.createElement('iframe');"
      "var html = '<body>NewFrame</body>';"
      "iframe.src = 'data:text/html;charset=utf-8,' + encodeURI(html);"
      "document.body.appendChild(iframe);");
  content::ExecuteScriptAsync(contents(), create_iframe_script);

  // Make sure the DidFinishNavigation occured.
  load_observer.Wait();
  PostToSelf();

  size_t j = 0;
  for (auto* frame : contents()->GetAllFrames()) {
    for (size_t i = 0; i < 20; ++i) {
      std::string create_image_script =
          std::string(
              "var image = document.createElement('img'); "
              "document.body.appendChild(image); image.src = '")
              .append(GetURL(std::string(kImagePrefix)
                                 .append(base::IntToString(++j))
                                 .append(".png';"))
                          .spec());

      EXPECT_TRUE(content::ExecuteScript(frame, create_image_script));
    }
  }

  // An image should not be fetched because subresource loading was paused in
  // both frames.
  EXPECT_EQ(0u, images_attempted());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlockNewFrameLoadResume) {
  // Tests that the image request loads normally when the page has not been
  // paused.

  EnableDataSaver(true);
  // Load a mostly empty page.
  NavigateToHeavyPage();
  // Pause subresource loading.
  ClickInfoBarLink();
  content::TestNavigationObserver load_observer(contents());

  // Adds an image to the page. It should be allowed to load.
  std::string create_iframe_script = std::string(
      "var iframe = document.createElement('iframe');"
      "var html = '<body>NewFrame</body>';"
      "iframe.src = 'data:text/html;charset=utf-8,' + encodeURI(html);"
      "document.body.appendChild(iframe);");
  content::ExecuteScriptAsync(contents(), create_iframe_script);

  // Make sure the DidFinishNavigation occured.
  load_observer.Wait();
  PostToSelf();

  for (auto* frame : contents()->GetAllFrames()) {
    if (contents()->GetMainFrame() == frame)
      continue;
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = '")
            .append(GetURL(std::string(kImagePrefix).append(".png';")).spec());
    ASSERT_TRUE(content::ExecuteScript(frame, create_image_script));
  }

  // An image should not be fetched because subresource loading was paused in
  // both frames.
  EXPECT_EQ(0u, images_attempted());

  // Previous image should be allowed to load now.
  ClickInfoBarLink();

  // An image should be fetched because subresource loading was resumed.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingInfobarShownAfterSamePageNavigation) {
  // Verifies that same page navigations do not dismiss the InfoBar.

  EnableDataSaver(true);
  // Load a page.
  NavigateToHeavyPage();

  ASSERT_EQ(1u, InfoBarCount());
  infobars::InfoBar* infobar =
      InfoBarService::FromWebContents(contents())->infobar_at(0);

  // Navigate on the page to an anchor.
  NavigateToHeavyPageAnchor("#anchor");

  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_EQ(infobar,
            InfoBarService::FromWebContents(contents())->infobar_at(0));
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingInfoBarNotShownAfterBlacklisted) {
  // Verifies the blacklist prevents over-showing the InfoBar.

  EnableDataSaver(true);
  // Load a page and ignore the InfoBar.
  NavigateToHeavyPage();
  ASSERT_EQ(1u, InfoBarCount());

  // Load a page and ignore the InfoBar.
  NavigateToHeavyPage();
  ASSERT_EQ(1u, InfoBarCount());

  // Load a page and due to session policy blacklisting, the InfoBar should not
  // show.
  NavigateToHeavyPage();
  ASSERT_EQ(0u, InfoBarCount());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       NavigationDataRemovedFromBlacklist) {
  // Verifies that clearing browsing data resets blacklist rules.

  EnableDataSaver(true);
  // Load a page and ignore the InfoBar.
  NavigateToHeavyPage();
  ASSERT_EQ(1u, InfoBarCount());

  // Load a page and ignore the InfoBar.
  NavigateToHeavyPage();
  ASSERT_EQ(1u, InfoBarCount());

  // Load a page and due to session policy blacklisting, the InfoBar should not
  // show.
  NavigateToHeavyPage();
  ASSERT_EQ(0u, InfoBarCount());

  // Clear the navigation history.
  content::BrowserContext::GetBrowsingDataRemover(browser()->profile())
      ->Remove(base::Time(), base::Time::Max(),
               ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
               content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);

  // After clearing history, the InfoBar should be allowed again.
  NavigateToHeavyPage();
  EXPECT_EQ(1u, InfoBarCount());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, IncognitoTest) {
  // Verifies the InfoBar is not shown in incognito.

  EnableDataSaver(true);
  auto* browser = CreateIncognitoBrowser();

  // Navigate to the page.
  NavigateToHeavyPageAnchorInBrowser(browser, std::string());

  EXPECT_EQ(0u, InfoBarService::FromWebContents(
                    browser->tab_strip_model()->GetActiveWebContents())
                    ->infobar_count());
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, DataSaverOffTest) {
  // Verifies that non-data saver users do not see the InfoBar.

  // Navigate to the page.
  NavigateToHeavyPage();

  EXPECT_EQ(0u, InfoBarCount());
}

class PageLoadCappingBrowserTestDismissAfterNetworkUse
    : public PageLoadCappingBrowserTest {
 public:
  PageLoadCappingBrowserTestDismissAfterNetworkUse() {}
  ~PageLoadCappingBrowserTestDismissAfterNetworkUse() override {}

  void ChangeParams(std::map<std::string, std::string>* params) override {
    (*params)["InfoBarTimeoutInMilliseconds"] = "50";
  }
};

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTestDismissAfterNetworkUse,
                       TestInfoBarDismiss) {
  // Verifies the InfoBar dismisses shortly (5ms) after the last resource is
  // loaded.
  EnableDataSaver(true);

  base::HistogramTester histogram_tester;

  // Load a page and ignore the InfoBar.
  NavigateToHeavyPage();

  // Verify the InfoBar was shown (it might be dismissed already by the
  // InfoBarTimeout logic).
  histogram_tester.ExpectBucketCount("HeavyPageCapping.InfoBarInteraction", 0,
                                     1);
  bool is_dismissed = histogram_tester.GetBucketCount(
                          "HeavyPageCapping.InfoBarInteraction", 3) > 0;
  if (!is_dismissed) {
    ASSERT_EQ(1u, InfoBarCount());
    WaitForInfoBarRemoved();
  }

  histogram_tester.ExpectBucketCount("HeavyPageCapping.InfoBarInteraction", 3,
                                     1);

  ASSERT_EQ(0u, InfoBarCount());
}
