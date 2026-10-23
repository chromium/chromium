// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tab_feature.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/core/keyed_service.h"
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
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
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

class SearchJourneyTabFeatureBrowserTest : public InProcessBrowserTest {
 public:
  SearchJourneyTabFeatureBrowserTest() {
    feature_list_.InitAndEnableFeature(kABASearchJourneys);
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

  SearchJourneyTracker* tracker() {
    return SearchJourneyTrackerFactory::GetForProfile(GetProfile());
  }

  TabListInterface* tab_list() { return TabListInterface::From(browser()); }

  // Opens a Google search for `query` in a new foreground tab and returns it.
  tabs::TabInterface* SearchInNewTab(const std::u16string& query) {
    const GURL search_url =
        template_url_service()->GenerateSearchURLForDefaultSearchProvider(
            query);
    EXPECT_TRUE(search_url.is_valid());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), search_url, WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return tab_list()->GetActiveTab();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       RecordsCopyAndArmsJourneyForCrossTabSearch) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));

  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  ASSERT_NE(SearchJourneyTabFeature::From(source_tab), nullptr);

  content::WebContents* source_contents = source_tab->GetContents();
  const SessionID source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_contents);
  const int source_nav_entry_id =
      source_contents->GetController().GetLastCommittedEntry()->GetUniqueID();

  CopyTextToClipboard(source_contents, "quantum entanglement physics");
  EXPECT_EQ(tracker()->GetRingBufferSizeForTesting(), 1u);
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());

  tabs::TabInterface* search_tab =
      SearchInNewTab(u"quantum entanglement physics");
  ASSERT_TRUE(search_tab);
  content::WebContents* search_contents = search_tab->GetContents();
  ASSERT_NE(search_contents, source_contents);
  const SessionID search_tab_id =
      sessions::SessionTabHelper::IdForTab(search_contents);

  const auto& journeys = tracker()->GetActiveJourneysForTesting();
  ASSERT_EQ(journeys.size(), 1u);
  auto it = journeys.find(source_tab_id);
  ASSERT_NE(it, journeys.end());
  EXPECT_EQ(it->second.source_nav_entry_id, source_nav_entry_id);
  EXPECT_EQ(it->second.search_tab_id, search_tab_id);
  EXPECT_EQ(it->second.search_terms, u"quantum entanglement physics");
  EXPECT_TRUE(it->second.matched_copy);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       ArmsJourneyWithoutCopy) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  const SessionID source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_tab->GetContents());

  ASSERT_TRUE(SearchInNewTab(u"how do tides work"));

  const auto& journeys = tracker()->GetActiveJourneysForTesting();
  ASSERT_EQ(journeys.size(), 1u);
  auto it = journeys.find(source_tab_id);
  ASSERT_NE(it, journeys.end());
  EXPECT_EQ(it->second.search_terms, u"how do tides work");
  EXPECT_FALSE(it->second.matched_copy);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       ReturningToSourceTabKeepsJourney) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  ASSERT_TRUE(SearchInNewTab(u"how do tides work"));
  ASSERT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);

  tab_list()->ActivateTab(source_tab->GetHandle());
  EXPECT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       VisitingAnotherTabDropsJourney) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  ASSERT_TRUE(SearchInNewTab(u"how do tides work"));
  ASSERT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com/other"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
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

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
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
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       KeepsJourneyWhenSearchTabClosed) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  const SessionID source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_tab->GetContents());

  tabs::TabInterface* search_tab = SearchInNewTab(u"how do tides work");
  ASSERT_TRUE(search_tab);
  ASSERT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);

  tab_list()->CloseTab(search_tab->GetHandle());
  const auto& journeys = tracker()->GetActiveJourneysForTesting();
  ASSERT_EQ(journeys.size(), 1u);
  auto it = journeys.find(source_tab_id);
  ASSERT_NE(it, journeys.end());
  EXPECT_TRUE(it->second.search_tab_closed);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       KeepsPromoParamsAcrossSameDocumentNavigation) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  const SessionID source_tab_id =
      sessions::SessionTabHelper::IdForTab(source_tab->GetContents());

  tabs::TabInterface* search_tab = SearchInNewTab(u"how do tides work");
  ASSERT_TRUE(search_tab);
  content::WebContents* search_contents = search_tab->GetContents();
  AimPromoParams params;
  params.mstk = "token";
  ASSERT_TRUE(tracker()->OnAimPromoParamsReceived(
      sessions::SessionTabHelper::IdForTab(search_contents),
      u"how do tides work", std::move(params), {}));

  // The results page updates its URL without loading a new document.
  content::TestNavigationObserver observer(search_contents);
  ASSERT_TRUE(content::ExecJs(
      search_contents,
      "history.replaceState(null, '', location.href + '&ved=1');"));
  observer.Wait();

  const auto& journeys = tracker()->GetActiveJourneysForTesting();
  auto it = journeys.find(source_tab_id);
  ASSERT_NE(it, journeys.end());
  ASSERT_TRUE(it->second.promo_params.has_value());
  EXPECT_EQ(it->second.promo_params->mstk, "token");
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       ClearsJourneyWhenSourceTabClosed) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
  ASSERT_TRUE(source_tab);
  ASSERT_TRUE(SearchInNewTab(u"how do tides work"));
  ASSERT_EQ(tracker()->GetActiveJourneysForTesting().size(), 1u);

  tab_list()->CloseTab(source_tab->GetHandle());
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

namespace {

constexpr char kAiPageUrl[] =
    "https://www.google.com/search?udm=50&sourceid=chrome";

AimPromoParams CreatePromoParams() {
  AimPromoParams params;
  params.mstk = "token";
  params.aioh = "3";
  params.csuir = "1";
  params.sxsrf = "xsrf";
  return params;
}

}  // namespace

class SearchJourneyRestoreBrowserTest
    : public SearchJourneyTabFeatureBrowserTest {
 public:
  SearchJourneyRestoreBrowserTest() {
    // Makes contextual tasks entry points eligible, so the side panel can open.
    eligibility_feature_list_.InitAndEnableFeature(
        kContextualTasksForceEntryPointEligibility);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SearchJourneyTabFeatureBrowserTest::SetUpBrowserContextKeyedServices(
        context);
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto ui_service =
              std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
                  Profile::FromBrowserContext(context), /*service=*/nullptr,
                  /*identity_manager=*/nullptr,
                  /*aim_eligibility_service=*/nullptr,
                  /*eligibility_manager=*/nullptr,
                  /*cookie_synchronizer=*/nullptr);
          ON_CALL(*ui_service, GetDefaultAiPageUrl())
              .WillByDefault(testing::Return(GURL(kAiPageUrl)));
          return ui_service;
        }));
  }

 protected:
  testing::NiceMock<MockContextualTasksUiService>* ui_service() {
    return static_cast<testing::NiceMock<MockContextualTasksUiService>*>(
        ContextualTasksUiServiceFactory::GetForBrowserContext(GetProfile()));
  }

  // Leaves a source tab for a search for "how do tides work" and, if `params`
  // is set, registers it along with a query hint. Returns the source tab. The
  // search tab stays active.
  tabs::TabInterface* StartJourney(std::optional<AimPromoParams> params) {
    if (!ui_test_utils::NavigateToURL(browser(),
                                      GURL("https://example.com/doc"))) {
      return nullptr;
    }
    tabs::TabInterface* source_tab = tab_list()->GetActiveTab();
    tabs::TabInterface* search_tab = SearchInNewTab(u"how do tides work");
    if (!source_tab || !search_tab) {
      return nullptr;
    }
    if (params) {
      EXPECT_TRUE(tracker()->OnAimPromoParamsReceived(
          sessions::SessionTabHelper::IdForTab(search_tab->GetContents()),
          u"how do tides work", std::move(*params),
          {AimPromoQueryHint::kEduHomework}));
    }
    return source_tab;
  }

  // Like `StartJourney()`, but then returns to the source tab.
  tabs::TabInterface* StartJourneyAndReturn(
      std::optional<AimPromoParams> params) {
    tabs::TabInterface* source_tab = StartJourney(std::move(params));
    if (source_tab) {
      tab_list()->ActivateTab(source_tab->GetHandle());
    }
    return source_tab;
  }

 private:
  base::test::ScopedFeatureList eligibility_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchJourneyRestoreBrowserTest,
                       RestoresJourneyInSidePanel) {
  tabs::TabInterface* source_tab = StartJourneyAndReturn(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  SearchJourneyTabFeature* feature = SearchJourneyTabFeature::From(source_tab);
  ASSERT_TRUE(feature);
  EXPECT_TRUE(feature->CanRestoreJourneyInSidePanel());

  // The query hint isn't added to the URL.
  EXPECT_CALL(
      *ui_service(),
      StartTaskUiInSidePanelImpl(
          testing::_, source_tab,
          GURL(std::string(kAiPageUrl) +
               "&q=how+do+tides+work&mstk=token&aioh=3&csuir=1&sxsrf=xsrf"
               "&aep=228"),
          testing::_,
          testing::AllOf(
              testing::Field(&StartTaskUiOptions::associate_web_contents,
                             false),
              testing::Field(
                  &StartTaskUiOptions::entry_point,
                  omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_ABA_ONRAMP_IPH),
              testing::Field(&StartTaskUiOptions::use_mstk_for_task_association,
                             true))));

  EXPECT_TRUE(feature->RestoreJourneyInSidePanel());
  EXPECT_FALSE(feature->CanRestoreJourneyInSidePanel());
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyRestoreBrowserTest,
                       RestoreUrlSkipsEmptyParams) {
  // The URL is always HTTPS, even if the AI page URL isn't.
  ON_CALL(*ui_service(), GetDefaultAiPageUrl())
      .WillByDefault(
          testing::Return(GURL("http://www.google.com/search?udm=50")));
  AimPromoParams params;
  params.mstk = "token";
  tabs::TabInterface* source_tab = StartJourneyAndReturn(std::move(params));
  ASSERT_TRUE(source_tab);

  EXPECT_CALL(*ui_service(),
              StartTaskUiInSidePanelImpl(
                  testing::_, source_tab,
                  GURL("https://www.google.com/search?udm=50"
                       "&q=how+do+tides+work&mstk=token&aep=228"),
                  testing::_, testing::_));

  EXPECT_TRUE(
      SearchJourneyTabFeature::From(source_tab)->RestoreJourneyInSidePanel());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyRestoreBrowserTest,
                       DoesNotRestoreWithoutPromoParams) {
  tabs::TabInterface* source_tab =
      StartJourneyAndReturn(/*params=*/std::nullopt);
  ASSERT_TRUE(source_tab);
  SearchJourneyTabFeature* feature = SearchJourneyTabFeature::From(source_tab);
  ASSERT_TRUE(feature);

  EXPECT_CALL(*ui_service(), StartTaskUiInSidePanelImpl).Times(0);

  EXPECT_FALSE(feature->CanRestoreJourneyInSidePanel());
  EXPECT_FALSE(feature->RestoreJourneyInSidePanel());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyRestoreBrowserTest,
                       DoesNotRestoreAfterSourceTabNavigates) {
  tabs::TabInterface* source_tab = StartJourneyAndReturn(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/new")));
  SearchJourneyTabFeature* feature = SearchJourneyTabFeature::From(source_tab);
  ASSERT_TRUE(feature);

  EXPECT_CALL(*ui_service(), StartTaskUiInSidePanelImpl).Times(0);

  EXPECT_FALSE(feature->CanRestoreJourneyInSidePanel());
  EXPECT_FALSE(feature->RestoreJourneyInSidePanel());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyRestoreBrowserTest,
                       DoesNotRestoreWhileSourceTabIsInactive) {
  tabs::TabInterface* source_tab = StartJourney(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  SearchJourneyTabFeature* feature = SearchJourneyTabFeature::From(source_tab);
  ASSERT_TRUE(feature);

  EXPECT_CALL(*ui_service(), StartTaskUiInSidePanelImpl).Times(0);

  EXPECT_FALSE(feature->CanRestoreJourneyInSidePanel());
  EXPECT_FALSE(feature->RestoreJourneyInSidePanel());
  EXPECT_FALSE(tracker()->GetActiveJourneysForTesting().empty());
}

class SearchJourneyTabFeatureDisabledBrowserTest : public InProcessBrowserTest {
 public:
  SearchJourneyTabFeatureDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kABASearchJourneys);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureDisabledBrowserTest,
                       NotCreatedWhenFeatureDisabled) {
  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_TRUE(tab);
  EXPECT_EQ(SearchJourneyTabFeature::From(tab), nullptr);
}

}  // namespace contextual_tasks
