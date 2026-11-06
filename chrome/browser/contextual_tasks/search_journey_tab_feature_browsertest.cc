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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
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
    ASSERT_TRUE(content::ExecJs(frame, content::JsReplace(
                                           R"(
      (function() {
        const textarea = document.createElement("textarea");
        textarea.id = "copy-target";
        textarea.value = $1;
        document.body.append(textarea);
        textarea.select();
      })();
    )",
                                           copied_text)));
    // Use WebContents::Copy() (EditorCommandSource::kMenuOrKeyBinding) rather
    // than document.execCommand("copy"), which silently no-ops in parallel
    // browser_tests when another browser window steals OS focus and
    // Document::hasFocus() becomes false.
    web_contents->Copy();
    waiter.Wait();
    ASSERT_TRUE(content::ExecJs(
        frame, "document.getElementById('copy-target')?.remove();"));
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
        browser(), search_url,
        ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
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

  // Returning while the profile is ineligible for Contextual Tasks records
  // ReturnedToSourceTab, but not Eligible.
  base::UserActionTester user_action_tester;
  tab_list()->ActivateTab(source_tab->GetHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.ReturnedToSourceTab"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Eligible"),
            0);
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

// Tests that an omnibox search for text the user copied tells Search that the
// query matches a copy.
IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       OmniboxSearchForCopiedTextAddsCopyMatchParam) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  content::WebContents* contents = tab_list()->GetActiveTab()->GetContents();
  CopyTextToClipboard(contents, "how do tides work");

  content::TestNavigationObserver observer(contents);
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "How do tides work");
  observer.Wait();

  std::string copy_match;
  EXPECT_TRUE(net::GetValueForKeyInQuery(observer.last_navigation_url(),
                                         "ictfc", &copy_match));
  EXPECT_EQ(copy_match, "1");
}

IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       OmniboxSearchForOtherTextOmitsCopyMatchParam) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  content::WebContents* contents = tab_list()->GetActiveTab()->GetContents();
  CopyTextToClipboard(contents, "how do tides work");

  content::TestNavigationObserver observer(contents);
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "why is the sky blue");
  observer.Wait();

  std::string query;
  ASSERT_TRUE(
      net::GetValueForKeyInQuery(observer.last_navigation_url(), "q", &query));
  EXPECT_EQ(query, "why is the sky blue");
  std::string copy_match;
  EXPECT_FALSE(net::GetValueForKeyInQuery(observer.last_navigation_url(),
                                          "ictfc", &copy_match));
}

// Tests that an Incognito omnibox search never tells Search that the query
// matches a copy, even one made in a regular window.
IN_PROC_BROWSER_TEST_F(SearchJourneyTabFeatureBrowserTest,
                       IncognitoOmniboxSearchOmitsCopyMatchParam) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/doc")));
  CopyTextToClipboard(tab_list()->GetActiveTab()->GetContents(),
                      "how do tides work");
  ASSERT_EQ(tracker()->GetRingBufferSizeForTesting(), 1u);

  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* incognito_contents =
      TabListInterface::From(incognito_browser)->GetActiveTab()->GetContents();
  content::TestNavigationObserver observer(incognito_contents);
  ui_test_utils::SendToOmniboxAndSubmit(incognito_browser, "how do tides work");
  observer.Wait();

  std::string query;
  ASSERT_TRUE(
      net::GetValueForKeyInQuery(observer.last_navigation_url(), "q", &query));
  EXPECT_EQ(query, "how do tides work");
  std::string copy_match;
  EXPECT_FALSE(net::GetValueForKeyInQuery(observer.last_navigation_url(),
                                          "ictfc", &copy_match));
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

// Turns on contextual cues, so tabs get the ABA search journey cue target.
class SearchJourneyCueBrowserTest : public SearchJourneyRestoreBrowserTest {
 public:
  SearchJourneyCueBrowserTest() {
    cueing_feature_list_.InitWithFeatures(
        {contextual_cueing::kContextualCueingV2,
         contextual_cueing::kContextualCueingV2MultiSource},
        {});
  }

 protected:
  contextual_cueing::CueTarget* GetCueTarget(tabs::TabInterface* tab) {
    auto* controller = contextual_cueing::ContextualCueingController::From(tab);
    return controller ? controller->GetTarget(
                            contextual_cueing::CueTargetType::kAbaSearchJourney)
                      : nullptr;
  }

  // Blocks loud cues, as after the user dismisses a cue, so only quiet
  // (omnibox chip) cues can show.
  void BlockLoudCues() {
    contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
        GetProfile())
        ->OnCueDismissed(contextual_cueing::CueTargetType::kGlic,
                         /*record_ucb_stats=*/false);
  }

  // Starts a journey, returns to the source tab, and only then registers
  // restore parameters so returning doesn't already show and use up the cue.
  tabs::TabInterface* StartJourneyAndReturnWithoutShowingCue() {
    tabs::TabInterface* source_tab = StartJourney(std::nullopt);
    tabs::TabInterface* search_tab = tab_list()->GetActiveTab();
    if (!source_tab || !search_tab) {
      return nullptr;
    }
    tab_list()->ActivateTab(source_tab->GetHandle());
    EXPECT_TRUE(tracker()->OnAimPromoParamsReceived(
        sessions::SessionTabHelper::IdForTab(search_tab->GetContents()),
        u"how do tides work", CreatePromoParams(),
        {AimPromoQueryHint::kEduHomework}));
    return source_tab;
  }

 private:
  base::test::ScopedFeatureList cueing_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest, EligibleOnlyOnceReturned) {
  tabs::TabInterface* source_tab = StartJourney(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  contextual_cueing::CueTarget* cue_target = GetCueTarget(source_tab);
  ASSERT_TRUE(cue_target);
  EXPECT_FALSE(cue_target->IsEligible());

  tab_list()->ActivateTab(source_tab->GetHandle());
  EXPECT_TRUE(cue_target->IsEligible());
}

IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest,
                       NotEligibleWithoutPromoParams) {
  tabs::TabInterface* source_tab = StartJourney(std::nullopt);
  ASSERT_TRUE(source_tab);
  contextual_cueing::CueTarget* cue_target = GetCueTarget(source_tab);
  ASSERT_TRUE(cue_target);

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  tab_list()->ActivateTab(source_tab->GetHandle());
  EXPECT_FALSE(cue_target->IsEligible());
  // Returning doesn't evaluate a cue that can't show.
  histogram_tester.ExpectTotalCount("ContextualCueing.V2.Decision", 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.ReturnedToSourceTab"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Eligible"),
            0);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest,
                       NotEligibleWhilePanelIsOpen) {
  tabs::TabInterface* source_tab = StartJourneyAndReturnWithoutShowingCue();
  ASSERT_TRUE(source_tab);
  contextual_cueing::CueTarget* cue_target = GetCueTarget(source_tab);
  ASSERT_TRUE(cue_target);
  ASSERT_TRUE(cue_target->IsEligible());

  SidePanelUI::From(browser())->Show(SidePanelEntryId::kContextualTasks);
  ContextualTasksPanelController* panel_controller =
      ContextualTasksPanelController::From(browser());
  ASSERT_TRUE(panel_controller);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return panel_controller->IsPanelOpenForContextualTask(); }));
  EXPECT_FALSE(cue_target->IsEligible());
}

// Tests that the cue falls back to a quiet omnibox chip when loud cues are
// blocked.
IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest,
                       ShowsAsChipWhenLoudCuesBlocked) {
  base::UserActionTester user_action_tester;
  BlockLoudCues();
  tabs::TabInterface* source_tab = StartJourney(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  page_actions::PageActionController* page_action_controller =
      page_actions::PageActionController::From(source_tab);
  ASSERT_TRUE(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  tab_list()->ActivateTab(source_tab->GetHandle());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().showing; }));
  EXPECT_FALSE(observer.GetCurrentPageActionState().anchored_message_showing);
  EXPECT_FALSE(GetCueTarget(source_tab)->IsEligible());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Eligible"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Shown"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Shown.Chip"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Shown.AnchoredMessage"),
            0);
}

IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest, GeneratesCueText) {
  tabs::TabInterface* source_tab = StartJourneyAndReturnWithoutShowingCue();
  ASSERT_TRUE(source_tab);
  contextual_cueing::CueTarget* cue_target = GetCueTarget(source_tab);
  ASSERT_TRUE(cue_target);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      eligibility;
  cue_target->CheckEligibility(source_tab->GetContents()->GetWeakPtr(),
                               contextual_cueing::CueIntrusiveness::kLoud,
                               eligibility.GetCallback());
  auto [eligible, generator] = eligibility.Take();
  ASSERT_TRUE(eligible);
  ASSERT_TRUE(generator);

  base::test::TestFuture<
      std::optional<optimization_guide::proto::ContextualCue>>
      cue;
  std::move(generator).Run(cue.GetCallback());
  ASSERT_TRUE(cue.Get().has_value());
  EXPECT_EQ(
      cue.Get()->anchored_message_cue().action_text(),
      l10n_util::GetStringUTF8(IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ACTION));
  EXPECT_EQ(cue.Get()->anchored_message_cue().anchored_message_text(),
            l10n_util::GetStringUTF8(
                IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ANCHORED_MESSAGE));
}

// Tests that the cue shows when the user returns to the source tab, and that
// clicking it restores the journey in the side panel.
IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest,
                       ShowsCueOnReturnAndRestoresJourneyOnClick) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  tabs::TabInterface* source_tab = StartJourney(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  page_actions::PageActionController* page_action_controller =
      page_actions::PageActionController::From(source_tab);
  ASSERT_TRUE(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  tab_list()->ActivateTab(source_tab->GetHandle());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));
  EXPECT_EQ(observer.GetCurrentPageActionState().tooltip,
            l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ACTION));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Eligible"),
            1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SearchJourney.Cue.Eligible.WordCount", 4, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Shown"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Shown.AnchoredMessage"),
            1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SearchJourney.Cue.Shown.WordCount", 4, 1);

  EXPECT_CALL(*ui_service(),
              StartTaskUiInSidePanelImpl(testing::_, source_tab, testing::_,
                                         testing::_, testing::_));
  actions::ActionItem* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);
  action->InvokeAction();
  EXPECT_TRUE(tracker()->GetActiveJourneysForTesting().empty());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Clicked"),
            1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.SearchJourney.Cue.Clicked.WordCount", 4, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Restored"),
            1);
}

// Tests that the cue shows at most once per journey, even if the user returns
// to the source tab again after it's gone.
IN_PROC_BROWSER_TEST_F(SearchJourneyCueBrowserTest, ShowsCueOncePerJourney) {
  tabs::TabInterface* source_tab = StartJourney(CreatePromoParams());
  ASSERT_TRUE(source_tab);
  tabs::TabInterface* search_tab = tab_list()->GetActiveTab();
  page_actions::PageActionController* page_action_controller =
      page_actions::PageActionController::From(source_tab);
  ASSERT_TRUE(page_action_controller);
  page_actions::PageActionObserver observer(kActionAnchoredContextualCue);
  observer.RegisterAsPageActionObserver(*page_action_controller);

  tab_list()->ActivateTab(source_tab->GetHandle());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));
  contextual_cueing::CueTarget* cue_target = GetCueTarget(source_tab);
  ASSERT_TRUE(cue_target);
  EXPECT_FALSE(cue_target->IsEligible());

  // The cue goes away, as when the user dismisses it, and then the user goes
  // back to the search tab and returns again.
  contextual_cueing::ContextualCueingController::From(source_tab)->HideCue();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().showing; }));
  tab_list()->ActivateTab(search_tab->GetHandle());
  tab_list()->ActivateTab(source_tab->GetHandle());

  EXPECT_FALSE(cue_target->IsEligible());
  EXPECT_FALSE(observer.GetCurrentPageActionState().showing);
  // The journey itself can still be restored.
  EXPECT_TRUE(SearchJourneyTabFeature::From(source_tab)
                  ->CanRestoreJourneyInSidePanel());
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
