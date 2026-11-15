// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSearchTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAiPageContentsId);

constexpr char16_t kQuery[] = u"how do tides work";
constexpr char kMstk[] = "test_mstk";

// Returns whether `url` is the AI page restoring the AI Overview of the search
// for `kQuery`. Browser tests cover the rest of the restore URL.
bool IsRestoreUrl(const GURL& url) {
  std::string query;
  std::string mstk;
  std::string entry_point;
  return url.host() == kMockAimPageHost && url.path() == "/search" &&
         net::GetValueForKeyInQuery(url, "q", &query) &&
         query == base::UTF16ToUTF8(kQuery) &&
         net::GetValueForKeyInQuery(url, "mstk", &mstk) && mstk == kMstk &&
         // `omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_ABA_ONRAMP_IPH`.
         net::GetValueForKeyInQuery(url, "aep", &entry_point) &&
         entry_point == "228";
}

}  // namespace

// Tests the core ABA search journey with real UI: the user leaves a page
// (Tab A), searches Google in another tab (Tab B), and returns to Tab A, where
// a contextual cue offers to restore the search's AI Overview in the
// Contextual Tasks side panel. Runs with the side panel rearchitecture both
// enabled and disabled.
class SearchJourneyInteractiveUiTest
    : public ContextualTasksInteractiveTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledFeatures();
    enabled_features.push_back({kABASearchJourneys, {}});
    // Shows every cue as the expanded anchored message, without Contextual
    // Cueing backing off.
    enabled_features.push_back(
        {contextual_cueing::kContextualCueingV2,
         {{contextual_cueing::kDisableCueBackoff.name, "true"}}});
    enabled_features.push_back(
        {contextual_cueing::kContextualCueingV2MultiSource, {}});
    std::vector<base::test::FeatureRef> disabled_features =
        GetDefaultDisabledFeatures();
    if (IsRearchitectureEnabled()) {
      std::erase(disabled_features, kContextualTasksRearchitecture);
      std::erase(disabled_features, kContextualTasksSidePanelRearchitecture);
      enabled_features.push_back({kContextualTasksRearchitecture, {}});
      enabled_features.push_back({kContextualTasksSidePanelRearchitecture, {}});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

 protected:
  bool IsRearchitectureEnabled() const { return GetParam(); }

  // Opens an HTTPS page in Tab A, then searches Google for `kQuery` in a new
  // foreground tab (Tab B), which arms a journey for Tab A. Tab B stays active.
  MultiStep StartJourney() {
    return Steps(
        InstrumentTab(kSourceTabId),
        NavigateWebContents(kSourceTabId, https_server_.GetURL("/title1.html")),
        AddInstrumentedTab(kSearchTabId, GetSearchUrl()),
        CheckElement(
            kSearchTabId,
            [this](ui::TrackedElement* el) {
              return RegisterPromoParams(
                  AsInstrumentedWebContents(el)->web_contents());
            },
            true));
  }

  // Waits for the cue to show on Tab A as the expanded anchored message.
  MultiStep WaitForCue() {
    return Steps(
        WaitForShow(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
        CheckViewProperty(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageLabelId,
            &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ANCHORED_MESSAGE)));
  }

  // Clicks the cue's button and checks that the side panel opens next to Tab A
  // with the journey's AI Overview restored as the first turn.
  MultiStep ClickCueAndCheckRestore() {
    return Steps(
        PressButton(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
        WaitForShow(kContextualTasksSidePanelWebViewElementId),
        WaitForHide(
            page_actions::AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
        InstrumentAiPage(),
        CheckElement(
            kAiPageContentsId,
            [](ui::TrackedElement* el) {
              return AsInstrumentedWebContents(el)
                  ->web_contents()
                  ->GetLastCommittedURL();
            },
            testing::Truly(&IsRestoreUrl)),
        CheckElement(
            kSourceTabId,
            [](ui::TrackedElement* el) {
              return tabs::TabInterface::GetFromContents(
                         AsInstrumentedWebContents(el)->web_contents())
                  ->IsActivated();
            },
            true),
        CheckResult(
            [this]() {
              return ContextualTasksPanelController::From(browser())
                  ->IsPanelOpenForContextualTask();
            },
            true, "The Contextual Tasks side panel is open."),
        CheckResult(
            [this]() {
              return tracker()->GetActiveJourneysForTesting().size();
            },
            0u, "The restored journey can't be restored again."));
  }

  // Instruments the side panel's AI page as `kAiPageContentsId`. The legacy
  // side panel shows a WebUI (`kSidePanelContentsId`) that embeds the AI page,
  // while the rearchitecture side panel shows the AI page directly.
  MultiStep InstrumentAiPage() {
    auto name_content_web_view =
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         });
    if (IsRearchitectureEnabled()) {
      return Steps(std::move(name_content_web_view),
                   InstrumentNonTabWebView(kAiPageContentsId,
                                           "SidePanelContentWebViewName"));
    }
    return Steps(
        std::move(name_content_web_view),
        InstrumentNonTabWebView(kSidePanelContentsId,
                                "SidePanelContentWebViewName"),
        InstrumentInnerWebContents(kAiPageContentsId, kSidePanelContentsId, 0));
  }

 private:
  GURL GetSearchUrl() {
    return TemplateURLServiceFactory::GetForProfile(browser()->GetProfile())
        ->GenerateSearchURLForDefaultSearchProvider(kQuery);
  }

  SearchJourneyTracker* tracker() {
    return SearchJourneyTrackerFactory::GetForProfile(browser()->GetProfile());
  }

  // Simulates the results page in `search_contents` registering restore
  // parameters for its AI Overview through
  // `contextualTasksPrivate.registerAimThreadForPromo`, which browser tests
  // cover.
  bool RegisterPromoParams(content::WebContents* search_contents) {
    AimPromoParams params;
    params.mstk = kMstk;
    return tracker()->OnAimPromoParamsReceived(
        sessions::SessionTabHelper::IdForTab(search_contents), kQuery,
        std::move(params), {AimPromoQueryHint::kEduHomework});
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       SelectingSourceTabThenClickingCueRestoresJourney) {
  base::UserActionTester user_action_tester;
  RunTestSequence(
      StartJourney(),
      // The cue isn't showing before the user returns to Tab A.
      EnsureNotPresent(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      SelectTab(kTabStripElementId, 0), WaitForCue(), ClickCueAndCheckRestore(),
      CheckResult(
          [&]() {
            return user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Cue.Clicked");
          },
          1, "Cue.Clicked is recorded once."),
      CheckResult(
          [&]() {
            return user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Restored");
          },
          1, "Restored is recorded once."));
}

// Closing Tab B re-activates Tab A, which counts as a return.
IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       ClosingSearchTabThenClickingCueRestoresJourney) {
  RunTestSequence(
      StartJourney(), Do([this]() {
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->CloseTab(tab_list->GetTab(1)->GetHandle());
      }),
      WaitForHide(kSearchTabId), WaitForCue(), ClickCueAndCheckRestore());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SearchJourneyInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RearchitectureEnabled"
                                             : "RearchitectureDisabled";
                         });

}  // namespace contextual_tasks
