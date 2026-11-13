// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
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
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_service.h"
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
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSourceTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSearchTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAiPageContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAiPageConnectedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSuggestedTabEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kOnboardingTooltipEvent);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kSourceTabSuggestedState);

constexpr char16_t kQuery[] = u"how do tides work";
constexpr char kMstk[] = "test_mstk";
constexpr char kCueIconName[] = "CueIcon";

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

  void SetUpLocalStatePrefService(PrefService* local_state) override {
    ContextualTasksInteractiveTestBase::SetUpLocalStatePrefService(local_state);
    // Turns off Omnibox Everywhere, which these tests don't use. Creating its
    // status icon after startup can hang browser startup on macOS.
    local_state->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEnabled, false);
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveTestBase::SetUpOnMainThread();
    WaitForTemplateURLServiceToLoad();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

  void TearDownOnMainThread() override {
    favicons_element_.reset();
    ContextualTasksInteractiveTestBase::TearDownOnMainThread();
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

  // Names the cue's icon in the omnibox `kCueIconName`. It shows the collapsed
  // cue as a chip, and the expanded cue is anchored to it.
  MultiStep NameCueIcon() {
    return Steps(NameViewRelative(
        kLocationBarElementId, kCueIconName,
        [](LocationBarView* location_bar) -> views::View* {
          return location_bar->page_action_container()->GetPageActionView(
              kActionAnchoredContextualCue);
        }));
  }

  // Checks that the side panel suggests Tab A as context for follow-ups, and
  // that the onboarding tooltip, which tells the user that the suggested tab
  // isn't shared yet, shows and hides once accepted.
  MultiStep CheckSourceTabSuggestedWithOnboardingTooltip() {
    if (IsRearchitectureEnabled()) {
      return Steps(
          WaitForSuggestedSourceTabInSession(),
          ShowSuggestedTabFaviconsFromAiPage(),
          WaitForShow(ContextualTasksOnboardingBubbleView::kBubbleElementId),
          PressButton(
              ContextualTasksOnboardingBubbleView::kAcceptButtonElementId),
          WaitForHide(ContextualTasksOnboardingBubbleView::kBubbleElementId));
    }
    return Steps(ReportComposeboxBoundsFromAiPage(),
                 WaitForSuggestedSourceTabInComposebox(),
                 WaitForWebUiOnboardingTooltip(/*shown=*/true),
                 ClickElement(kSidePanelContentsId,
                              {"contextual-tasks-app", "#onboardingTooltip",
                               "cr-button.action-button"}),
                 WaitForWebUiOnboardingTooltip(/*shown=*/false));
  }

  SearchJourneyTracker* tracker() {
    return SearchJourneyTrackerFactory::GetForProfile(browser()->GetProfile());
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  GURL GetSearchUrl() {
    return TemplateURLServiceFactory::GetForProfile(browser()->GetProfile())
        ->GenerateSearchURLForDefaultSearchProvider(kQuery);
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

  // Rearchitecture side panel only. Waits for the AI page to suggest the active
  // tab, Tab A, through the component extension. The suggested tab is pending:
  // it isn't uploaded unless the user sends a follow-up with it.
  MultiStep WaitForSuggestedSourceTabInSession() {
    return Steps(
        PollState(
            kSourceTabSuggestedState,
            [this]() {
              contextual_search::ContextualSearchSessionHandle* session_handle =
                  ContextualTasksPanelController::From(browser())
                      ->GetContextualSearchSessionHandleForPanel();
              if (!session_handle) {
                return false;
              }
              const int32_t source_tab_id =
                  sessions::SessionTabHelper::IdForTab(
                      TabListInterface::From(browser())
                          ->GetTab(0)
                          ->GetContents())
                      .id();
              return std::ranges::any_of(
                  session_handle->GetTabContextState().attached,
                  [source_tab_id](const contextual_search::TabInfo& tab) {
                    return tab.tab_id == source_tab_id &&
                           tab.pending_delayed_upload && !tab.uploaded;
                  });
            }),
        WaitForState(kSourceTabSuggestedState, true),
        StopObservingState(kSourceTabSuggestedState));
  }

  // Rearchitecture side panel only. Simulates the AI page showing the favicons
  // of the suggested tabs, which the onboarding tooltip is anchored to. The
  // mock AI page doesn't render them.
  MultiStep ShowSuggestedTabFaviconsFromAiPage() {
    return Steps(Do([this]() {
      favicons_element_ = std::make_unique<ui::test::TestElement>(
          kContextualTasksFaviconsElementId,
          BrowserElements::From(browser())->GetContext());
      favicons_element_->SetNativeView(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetWidget()
              ->GetNativeView());
      favicons_element_->SetScreenBounds(gfx::Rect(100, 100, 50, 50));
      favicons_element_->Show();
    }));
  }

  // Legacy side panel only. Has the AI page report where to show the
  // composebox, as the real AI page does once it loads. Until then, the side
  // panel keeps the composebox hidden on a restored thread. Like the real AI
  // page, it reports the bottom of its viewport, so that the composebox and the
  // onboarding tooltip are visible whatever the window size.
  MultiStep ReportComposeboxBoundsFromAiPage() {
    StateChange connected;
    connected.type = StateChange::Type::kConditionTrue;
    connected.test_function = "() => !!window.__ctWebuiSourceReady";
    connected.event = kAiPageConnectedEvent;
    return Steps(WaitForStateChange(kAiPageContentsId, connected),
                 ExecuteJs(kAiPageContentsId, R"(
                   () => {
                     const height = 80;
                     const width = window.innerWidth;
                     const bottom = window.innerHeight;
                     webuiSource.postMessage({
                       type: 'input-plate-bounds-update',
                       'bounds-rect': {top: bottom - height, left: 0, width,
                                       height, right: width, bottom},
                       occluders: [],
                     }, webuiOrigin);
                   }
                 )"));
  }

  // Legacy side panel only. Waits for the side panel's composebox to suggest
  // the active tab, Tab A, as context.
  MultiStep WaitForSuggestedSourceTabInComposebox() {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app", "#composebox"};
    change.test_function =
        "(el) => el.getComposebox()?.getHasAutomaticActiveTabChipToken()";
    change.event = kSuggestedTabEvent;
    return Steps(WaitForStateChange(kSidePanelContentsId, change));
  }

  // Legacy side panel only. Waits for the onboarding tooltip in the side
  // panel's WebUI to be shown or hidden.
  MultiStep WaitForWebUiOnboardingTooltip(bool shown) {
    StateChange change;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.where = {"contextual-tasks-app", "#onboardingTooltip"};
    change.test_function =
        shown ? "(el) => el.shouldShow" : "(el) => !el.shouldShow";
    change.event = kOnboardingTooltipEvent;
    return Steps(WaitForStateChange(kSidePanelContentsId, change));
  }

  std::unique_ptr<ui::test::TestElement> favicons_element_;
};

// This tests the following CUJ:
//  (1) User is on a web page (Tab A) and searches Google in a new tab (Tab B),
//      whose results page registers its AI Overview for restore.
//  (2) User selects Tab A, and the contextual cue shows as the expanded
//      anchored message.
//  (3) User clicks the cue, which opens the Contextual Tasks side panel next to
//      Tab A with the search's AI Overview restored as the first turn.
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

// This tests the following CUJ:
//  (1) User is on a web page (Tab A) and searches Google in a new tab (Tab B),
//      whose results page registers its AI Overview for restore.
//  (2) User closes Tab B, which re-activates Tab A and counts as a return, and
//      the contextual cue shows as the expanded anchored message.
//  (3) User clicks the cue, which opens the Contextual Tasks side panel next to
//      Tab A with the search's AI Overview restored as the first turn.
IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       ClosingSearchTabThenClickingCueRestoresJourney) {
  RunTestSequence(
      StartJourney(), Do([this]() {
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->CloseTab(tab_list->GetTab(1)->GetHandle());
      }),
      WaitForHide(kSearchTabId), WaitForCue(), ClickCueAndCheckRestore());
}

// This tests the following CUJ:
//  (1) User is on a web page (Tab A), searches Google in a new tab (Tab B), and
//      returns to Tab A, where the contextual cue shows as the expanded
//      anchored message.
//  (2) User clicks the cue's icon in the omnibox, which collapses the cue into
//      a chip, as the cue's timeout does.
//  (3) User clicks the chip, which expands the cue again without restoring the
//      journey.
//  (4) User clicks the cue, which restores the search's AI Overview in the
//      Contextual Tasks side panel.
IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       ClickingCollapsedChipExpandsCueThenRestoresJourney) {
  base::UserActionTester user_action_tester;
  RunTestSequence(
      StartJourney(), SelectTab(kTabStripElementId, 0), WaitForCue(),
      NameCueIcon(), PressButton(kCueIconName),
      WaitForHide(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      // The cue collapses into the icon's chip rather than hiding.
      CheckView(
          kCueIconName,
          [](page_actions::PageActionView* icon) {
            return icon->IsChipVisible();
          },
          true),
      PressButton(kCueIconName), WaitForCue(),
      CheckResult(
          [&]() {
            return user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.Restored");
          },
          0, "Expanding the chip doesn't restore the journey."),
      ClickCueAndCheckRestore(),
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

// This tests the following CUJ:
//  (1) User is on a web page (Tab A) and searches Google in a new tab (Tab B),
//      whose results page registers its AI Overview for restore.
//  (2) While the user is in Tab B, Tab A navigates to another page.
//  (3) User selects Tab A. Since it's no longer on the page the user left, this
//      doesn't count as a return: the journey is dropped and no cue shows.
IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       ReturningToSourceTabOnAnotherPageShowsNoCue) {
  base::UserActionTester user_action_tester;
  RunTestSequence(
      StartJourney(),
      NavigateWebContents(kSourceTabId, https_server_.GetURL("/title2.html")),
      SelectTab(kTabStripElementId, 0),
      // Tab A is on another page, so activating it drops the journey. With no
      // restorable journey, the cue target is ineligible and no cue can show.
      CheckResult(
          [this]() { return tracker()->GetActiveJourneysForTesting().size(); },
          0u, "The journey is dropped."),
      EnsureNotPresent(
          page_actions::AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckResult(
          [&]() {
            return user_action_tester.GetActionCount(
                "ContextualTasks.SearchJourney.ReturnedToSourceTab");
          },
          0, "ReturnedToSourceTab isn't recorded."));
}

// This tests the following CUJ:
//  (1) User is on a web page (Tab A), searches Google in a new tab (Tab B), and
//      returns to Tab A, where the contextual cue shows.
//  (2) User clicks the cue, which restores the search's AI Overview in the
//      Contextual Tasks side panel.
//  (3) The side panel suggests Tab A as context for follow-ups without sharing
//      it, and the onboarding tooltip tells the user that the tab isn't shared
//      yet.
//  (4) User accepts the tooltip, which hides it.
IN_PROC_BROWSER_TEST_P(SearchJourneyInteractiveUiTest,
                       RestoringJourneySuggestsSourceTabAndShowsTooltip) {
  RunTestSequence(StartJourney(), SelectTab(kTabStripElementId, 0),
                  WaitForCue(), ClickCueAndCheckRestore(),
                  CheckSourceTabSuggestedWithOnboardingTooltip());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SearchJourneyInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RearchitectureEnabled"
                                             : "RearchitectureDisabled";
                         });

}  // namespace contextual_tasks
