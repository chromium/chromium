// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/json/string_escape.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
// TabUnderlineView currently resides in glic/browser_ui.
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"

namespace contextual_tasks {

namespace {

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
// WebContents of the two WebUI omnibox popup surfaces. The classic popup hosts
// the suggestion list plus the context entrypoint; selecting a tab from its
// context menu swaps it for the AIM popup, which hosts the composebox.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAimPopupWebContentsId);
// Whether the browser-side omnibox has focus; autocomplete only runs while it
// does, so the popup will not open until this is true.
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kOmniboxFocusState);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMenuOpenEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFlyoutVisibleEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCoinsReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTriggerReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCheckedReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSubmitEnabledEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputClearedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUploadsCompleteEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRenderedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimSubmitEnabledEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimUploadsCompleteEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimCoinsShownEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kZeroStateChangedEvent);

}  // namespace

class ContextualTasksContextManagementInteractiveTestBase
    : public ContextualTasksInteractiveTestBase {
 public:
  ContextualTasksContextManagementInteractiveTestBase() = default;
  ~ContextualTasksContextManagementInteractiveTestBase() override = default;

  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledFeatures();
    enabled_features.push_back(
        {kContextualTasksForceEntryPointEligibility, {}});
    enabled_features.push_back({omnibox::kContextManagementInComposebox,
                                {{"enable_tab_deselection", "true"}}});
    enabled_features.push_back({omnibox::kTabFaviconChipsToCoins, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                GetDefaultDisabledFeatures());
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveTestBase::SetUpOnMainThread();
    SidePanelUI::From(browser())->DisableAnimationsForTesting();

    url_loader_interceptor_.reset();
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [this](content::URLLoaderInterceptor::RequestParams* params) {
              const GURL& url = params->url_request.url;
              if (url.host().find("lensfrontend-pa") != std::string::npos) {
                if (url.path().find("gsessionid") != std::string::npos) {
                  lens::LensOverlayServerClusterInfoResponse cluster_info;
                  cluster_info.set_server_session_id("fake_server_session_id");
                  cluster_info.set_search_session_id("fake_search_session_id");
                  std::string response_data;
                  cluster_info.SerializeToString(&response_data);
                  content::URLLoaderInterceptor::WriteResponse(
                      "HTTP/1.1 200 OK\r\nContent-Type: "
                      "application/x-protobuf\r\n\r\n",
                      response_data, params->client.get());
                  return true;
                }
                if (fail_context_uploads_) {
                  content::URLLoaderInterceptor::WriteResponse(
                      "HTTP/1.1 500 Internal Server Error\r\n\r\n", "",
                      params->client.get());
                  return true;
                }
                // Keep the context upload in flight by retaining the client
                // without ever responding. Dropping it would complete the
                // request with an error instead.
                if (ShouldStallContextUploads()) {
                  stalled_upload_clients_.push_back(std::move(params->client));
                  return true;
                }
                lens::LensOverlayServerResponse upload_response;
                std::string response_data;
                upload_response.SerializeToString(&response_data);
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\r\nContent-Type: "
                    "application/x-protobuf\r\n\r\n",
                    response_data, params->client.get());
                return true;
              }
              if (url.host() == "www.google.com" ||
                  (url.host() == "127.0.0.1" && url.path() == "/search")) {
                content::URLLoaderInterceptor::WriteResponse(
                    "chrome/test/data/mock_aim_page.html",
                    params->client.get());
                return true;
              }
              return false;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ContextualTasksInteractiveTestBase::TearDownOnMainThread();
  }

 protected:
  // --- DeepQuery Paths ---
  const DeepQuery kComposeboxContainer = {"contextual-tasks-app", "#composebox",
                                          "#composebox"};

  const DeepQuery kContextEntrypoint = {
      "contextual-tasks-app", "#composebox",       "#composebox",
      "#contextEntrypoint",   "#entrypointButton", "#entrypoint"};

  const DeepQuery kContextMenu = {"contextual-tasks-app", "#composebox",
                                  "#composebox", "#contextEntrypoint", "#menu"};

  const DeepQuery kShareTabsTrigger = {
      "contextual-tasks-app", "#composebox", "#composebox",
      "#contextEntrypoint",   "#menu",       "#shareTabsTrigger"};

  const DeepQuery kShareTabsFlyout = {
      "contextual-tasks-app", "#composebox", "#composebox",
      "#contextEntrypoint",   "#menu",       ".share-tabs-flyout"};

  const DeepQuery kComposeboxInput = {"contextual-tasks-app", "#composebox",
                                      "#composebox", "#composeboxInput",
                                      "#input"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  const DeepQuery kNewThreadButton = {"contextual-tasks-app", "#toolbar",
                                      "#newThreadButton"};

  const DeepQuery kThreadFrame = {"contextual-tasks-app", "#threadFrame"};

  // --- Kombucha Macro Helpers ---

  static auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                                   const DeepQuery& element) {
    StateChange change;
    change.type = StateChange::Type::kExists;
    change.where = element;
    change.event = kElementExistsEvent;
    return WaitForStateChange(contents_id, change);
  }

  // Asynchronously waits until the composebox has updated its added tabs set
  // and all background tab context uploads have completely finished.
  auto WaitForFileUploadsComplete(const ui::ElementIdentifier& contents_id,
                                  int expected_tabs_count) {
    StateChange uploads_done;
    uploads_done.type = StateChange::Type::kExistsAndConditionTrue;
    uploads_done.where = kComposeboxContainer;
    uploads_done.test_function = base::StringPrintf(
        R"(el => {
          const count = el.addedTabsIds ? el.addedTabsIds.size : 0;
          return count === %d && el.fileUploadsComplete === true;
        })",
        expected_tabs_count);
    uploads_done.event = kUploadsCompleteEvent;
    return WaitForStateChange(contents_id, uploads_done);
  }

  // Runs the message loop with `kNestableTasksAllowed` until `condition`
  // evaluates to true. `base::test::RunUntil` uses `RunLoop::Type::kDefault`,
  // which blocks application tasks when called inside an `InteractionSequence`
  // `Do()` step.
  bool RunUntilNestable(base::FunctionRef<bool()> condition) {
    if (condition()) {
      return true;
    }
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(10),
                base::BindLambdaForTesting([&]() {
                  if (condition()) {
                    timer.Stop();
                    run_loop.Quit();
                  }
                }));
    run_loop.Run();
    return condition();
  }

  // Asynchronously verifies whether the native tab strip reflects the expected
  // set of underlined tabs.
  auto VerifyUnderlinedTabs(const std::set<int>& expected_indices) {
    return Do([this, expected_indices]() {
      EXPECT_TRUE(RunUntilNestable([&]() {
        auto* tabstrip = BrowserView::GetBrowserViewForBrowser(browser())
                             ->horizontal_tab_strip_for_testing();
        if (!tabstrip) {
          return false;
        }
        for (int i = 0; i < tabstrip->GetTabCount(); ++i) {
          auto* tab = tabstrip->tab_at(i);
          if (!tab) {
            return false;
          }
          auto* underline = tab->glic_underline();
          bool is_showing = underline && underline->IsShowing();
          bool should_underline = expected_indices.contains(i);
          if (is_showing != should_underline) {
            return false;
          }
        }
        return true;
      })) << "Tab strip underlines did not match the expected set of indices.";
    });
  }

  // Opens the share tabs flyout menu inside the side panel composebox.
  // Note: Direct mutation of hover/timer properties (e.g. pointerOverFlyout_,
  // cancelCloseTimer_) is intentionally used here to bypass asynchronous hover
  // delay timers in the WebUI and prevent flakiness under test runners.
  auto OpenShareTabsFlyout(const ui::ElementIdentifier& contents_id) {
    StateChange menu_open;
    menu_open.type = StateChange::Type::kExistsAndConditionTrue;
    menu_open.where = kContextMenu;
    menu_open.test_function = "el => !!el.open";
    menu_open.event = kMenuOpenEvent;

    StateChange flyout_visible;
    flyout_visible.type = StateChange::Type::kExistsAndConditionTrue;
    flyout_visible.where = kShareTabsFlyout;
    flyout_visible.test_function = "el => !el.hidden";
    flyout_visible.event = kFlyoutVisibleEvent;

    return Steps(WaitForElementExists(contents_id, kContextEntrypoint),
                 ExecuteJsAt(contents_id, kContextEntrypoint,
                             R"(el => {
                      const container = el.getRootNode().host?.getRootNode();
                      const menu = container?.querySelector('#menu');
                      if (menu && !menu.open) {
                        el.click();
                      }
                    })"),
                 WaitForStateChange(contents_id, menu_open),
                 WaitForElementExists(contents_id, kShareTabsTrigger),
                 ExecuteJsAt(contents_id, kShareTabsTrigger,
                             R"(el => {
              el.dispatchEvent(
                  new PointerEvent('pointerenter', {bubbles: true}));
              const menu = el.getRootNode().host;
              if (menu) {
                menu.pointerOverFlyout_ = true;
                menu.pointerOverTrigger_ = true;
                if (typeof menu.cancelCloseTimer_ === 'function') {
                  menu.cancelCloseTimer_();
                }
                if (typeof menu.onShareTabsRowPointerenter_ === 'function') {
                  menu.onShareTabsRowPointerenter_();
                }
                if (typeof menu.setShareTabsFlyoutOpen_ === 'function') {
                  menu.setShareTabsFlyoutOpen_(true);
                } else {
                  menu.shareTabsFlyoutOpen = true;
                }
                if (typeof menu.updateFlyoutPosition_ === 'function') {
                  menu.updateFlyoutPosition_();
                }
              }
            })"),
                 WaitForStateChange(contents_id, flyout_visible));
  }

  // JavaScript snippet to locate a tab button in the flyout menu matching
  // against favicon URL, tab title, button title, or aria-label.
  static constexpr char kFindFlyoutTabButtonJs[] = R"(
    const findTabBtn = (container, matcher) => {
      const btns = Array.from(
          container.querySelectorAll('button.dropdown-item'));
      return btns.find(b => {
        const favicon = b.querySelector('cr-composebox-tab-favicon');
        const faviconUrl = (favicon?.url?.url || favicon?.url || '');
        const title = b.querySelector('.tab-title')?.textContent || '';
        const btnTitle = b.title || '';
        const ariaLabel = b.getAttribute('aria-label') || '';
        const allText = `${faviconUrl} ${title} ${btnTitle} ${ariaLabel}`;
        return allText.includes(matcher);
      });
    };
  )";

  // Toggles the selection of a tab in the flyout menu by matching its URL or
  // title.
  auto ToggleFlyoutTab(const ui::ElementIdentifier& contents_id,
                       std::string_view matcher) {
    return Steps(OpenShareTabsFlyout(contents_id),
                 ExecuteJsAt(contents_id, kShareTabsFlyout,
                             base::StringPrintf(
                                 R"(el => {
                  %s
                  const btn = findTabBtn(el, %s);
                  if (!btn) {
                    throw new Error(
                        'ToggleFlyoutTab: tab button not found for: ' + %s);
                  }
                  btn.click();
                })",
                                 kFindFlyoutTabButtonJs,
                                 base::GetQuotedJSONString(matcher).c_str(),
                                 base::GetQuotedJSONString(matcher).c_str())));
  }

  // Verifies the checked state and check icon for a flyout menu item matching
  // URL or title.
  auto VerifyFlyoutTabChecked(const ui::ElementIdentifier& contents_id,
                              std::string_view matcher,
                              bool expected_checked) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kShareTabsFlyout;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          %s
          const btn = findTabBtn(el, %s);
          if (!btn) {
            return false;
          }
          const isChecked = btn.getAttribute('aria-checked') === 'true';
          const hasCheck = !!btn.querySelector('.share-tabs-check');
          return isChecked === %s && hasCheck === %s;
        }
        )",
        kFindFlyoutTabButtonJs, base::GetQuotedJSONString(matcher).c_str(),
        expected_checked ? "true" : "false",
        expected_checked ? "true" : "false");
    state.event = kCheckedReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies the context menu trigger row text ("Add Tabs" vs "Sharing n tabs")
  // and inline coins.
  auto VerifyMenuTriggerState(const ui::ElementIdentifier& contents_id,
                              int expected_count) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kShareTabsTrigger;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const title =
              el.querySelector('.tab-title')?.textContent?.trim() || '';
          const coins = el.querySelector('composebox-favicon-group');
          if (%d === 0) {
            const isAddOrShare =
                title.includes('Share') || title.includes('Add');
            const noCoins = !coins || coins.tabs?.length === 0;
            return isAddOrShare && noCoins;
          } else {
            return title.includes('Sharing') && coins &&
                   coins.tabs?.length === %d;
          }
        }
        )",
        expected_count, expected_count);
    state.event = kTriggerReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies the count of coin favicons rendered inside the Plus button.
  auto VerifyPlusButtonCoins(const ui::ElementIdentifier& contents_id,
                             int expected_count) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kComposeboxContainer;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const root = el.shadowRoot || el;
          const entrypoint = root.querySelector('#contextEntrypoint');
          const button =
              entrypoint?.shadowRoot?.querySelector('#entrypointButton');
          const coins =
              button?.shadowRoot?.querySelector('composebox-favicon-group');
          if (%d === 0) {
            return !coins || !coins.tabs || coins.tabs.length === 0;
          } else {
            return !!coins && !!coins.tabs && coins.tabs.length === %d;
          }
        }
        )",
        expected_count, expected_count);
    state.event = kCoinsReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies that the coin favicons rendered inside the Plus button match the
  // exact set of `expected_matchers` (checked against each tab's URL or title).
  auto VerifyPlusButtonCoinTabs(
      const ui::ElementIdentifier& contents_id,
      std::initializer_list<std::string_view> expected_matchers) {
    std::vector<std::string> quoted_matchers;
    for (std::string_view matcher : expected_matchers) {
      quoted_matchers.push_back(base::GetQuotedJSONString(matcher));
    }
    const std::string matchers_json =
        "[" + base::JoinString(quoted_matchers, ",") + "]";

    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kComposeboxContainer;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const root = el.shadowRoot || el;
          const entrypoint = root.querySelector('#contextEntrypoint');
          const button =
              entrypoint?.shadowRoot?.querySelector('#entrypointButton');
          const coins =
              button?.shadowRoot?.querySelector('composebox-favicon-group');
          const tabs = coins?.tabs || [];
          const expected = %s;
          if (tabs.length !== expected.length) {
            return false;
          }
          return expected.every(matcher =>
              tabs.some(tab => {
                const url = tab?.url?.url || tab?.url || '';
                const title = tab?.title || '';
                return `${url} ${title}`.includes(matcher);
              }));
        }
        )",
        matchers_json.c_str());
    state.event = kCoinsReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Types and submits a text query in the side panel composebox.
  auto SubmitSidePanelQuery(const ui::ElementIdentifier& contents_id,
                            const std::string& query) {
    const DeepQuery kSubmitButtonHost = {"contextual-tasks-app", "#composebox",
                                         "#composebox", "cr-composebox-submit"};

    StateChange submit_enabled;
    submit_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    submit_enabled.where = kSubmitButtonHost;
    submit_enabled.test_function = "el => !el.disabled";
    submit_enabled.event = kSubmitEnabledEvent;

    StateChange input_cleared;
    input_cleared.type = StateChange::Type::kExistsAndConditionTrue;
    input_cleared.where = kComposeboxInput;
    input_cleared.test_function = "el => !el.value || el.value === ''";
    input_cleared.event = kInputClearedEvent;

    return Steps(WaitForElementExists(contents_id, kComposeboxInput),
                 ExecuteJsAt(contents_id, kComposeboxInput,
                             base::StringPrintf(
                                 R"((el) => {
                  el.value = %s;
                  el.dispatchEvent(new Event('input', { bubbles: true }));
                  el.dispatchEvent(new Event('change', { bubbles: true }));
                })",
                                 base::GetQuotedJSONString(query).c_str())),
                 WaitForStateChange(contents_id, submit_enabled),
                 ExecuteJsAt(contents_id, kSubmitButton, "el => el.click()"),
                 WaitForStateChange(contents_id, input_cleared));
  }

  // Commits an AIM thread URL with `q` and `mtid` inside `#threadFrame` so
  // `ContextualTasksUI::FrameNavObserver` transitions the current task out of
  // zero state and associates it with `thread_id`.
  auto CommitActiveThreadUrlInSidePanel(
      const ui::ElementIdentifier& contents_id,
      const std::string& query,
      const std::string& thread_id) {
    StateChange non_zero_state;
    non_zero_state.type = StateChange::Type::kExistsAndConditionTrue;
    non_zero_state.where = {"contextual-tasks-app"};
    non_zero_state.test_function = "el => el.isZeroState_ === false";
    non_zero_state.event = kZeroStateChangedEvent;

    const std::string thread_url =
        base::StringPrintf("https://www.google.com/search?udm=50&q=%s&mtid=%s",
                           query.c_str(), thread_id.c_str());

    return Steps(WaitForElementExists(contents_id, kThreadFrame),
                 ExecuteJsAt(contents_id, kThreadFrame,
                             base::StringPrintf(
                                 "el => { el.src = %s; }",
                                 base::GetQuotedJSONString(thread_url).c_str()),
                             ExecuteJsMode::kFireAndForget),
                 WaitForStateChange(contents_id, non_zero_state));
  }

  // Clicks the New Thread button in the side panel toolbar and waits for the
  // app to transition back to zero state on the new thread.
  auto StartNewThreadFromSidePanel(const ui::ElementIdentifier& contents_id) {
    StateChange zero_state;
    zero_state.type = StateChange::Type::kExistsAndConditionTrue;
    zero_state.where = {"contextual-tasks-app"};
    zero_state.test_function = "el => el.isZeroState_ === true";
    zero_state.event = kZeroStateChangedEvent;

    return Steps(WaitForElementExists(contents_id, kNewThreadButton),
                 ExecuteJsAt(contents_id, kNewThreadButton, "el => el.click()",
                             ExecuteJsMode::kFireAndForget),
                 WaitForStateChange(contents_id, zero_state));
  }

  // Shows the Contextual Tasks side panel with kOmniboxPageAction invocation
  // source. Setting kOmniboxPageAction ensures that
  // ContextualTasksUI::ShouldClearAllInputsOnSubmit evaluates to false,
  // preventing submitted tab tokens and input context from being dropped across
  // turns in multi-turn flows.
  auto ShowSidePanel() {
    return Do([this]() {
      if (auto* lens_controller = LensSearchController::FromTabWebContents(
              browser()->GetActiveTabInterface()->GetContents())) {
        lens_controller->SetInvocationSource(
            lens::LensOverlayInvocationSource::kOmniboxPageAction);
      }
      auto* coordinator = ContextualTasksPanelController::From(browser());
      coordinator->Show(
          false, omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
    });
  }

  // Shows the Contextual Tasks side panel and instruments its WebContents.
  auto OpenSidePanelWithWebContents() {
    return Steps(
        ShowSidePanel(), WaitForShow(kContextualTasksSidePanelWebViewElementId),
        NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                         "SidePanelContentWebViewName",
                         [](ContextualTasksWebView* web_view) -> views::View* {
                           return web_view->content_web_view();
                         }),
        InstrumentNonTabWebView(kSidePanelWebContentsId,
                                "SidePanelContentWebViewName"),
        WaitForElementExists(kSidePanelWebContentsId, kComposeboxContainer));
  }

  // Returns the task ID that the tab at `index` is associated with in
  // ContextualTasksService, or an invalid Uuid if it has no task. This is the
  // association that drives whether the side panel follows the user to a tab;
  // it is distinct from the tab-strip underline, which reflects the session's
  // uploaded context attachments.
  base::Uuid TaskIdForTab(int index) {
    content::WebContents* contents =
        browser()->GetAllTabInterfaces()[index]->GetContents();
    std::optional<ContextualTask> task =
        ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile())
            ->GetContextualTaskForTab(
                sessions::SessionTabHelper::IdForTab(contents));
    return task ? task->GetTaskId() : base::Uuid();
  }

  // When true, context uploads are held open indefinitely instead of
  // completing immediately. The production interceptor answers uploads
  // synchronously, which hides any behavior that depends on a tab being
  // attached while its upload is still in flight.
  virtual bool ShouldStallContextUploads() const { return false; }

  void FailStalledUploads() {
    EXPECT_TRUE(
        RunUntilNestable([this]() { return !stalled_upload_clients_.empty(); }));
    fail_context_uploads_ = true;
    for (auto& client : stalled_upload_clients_) {
      content::URLLoaderInterceptor::WriteResponse(
          "HTTP/1.1 500 Internal Server Error\r\n\r\n", "", client.get());
    }
    stalled_upload_clients_.clear();
  }

 private:
  bool fail_context_uploads_ = false;
  std::vector<mojo::Remote<network::mojom::URLLoaderClient>>
      stalled_upload_clients_;
};

class ContextualTasksContextManagementInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksContextManagementInteractiveUiTest() = default;
  ~ContextualTasksContextManagementInteractiveUiTest() override = default;

  UserVariation GetUserVariation() const override { return GetParam(); }
};

// --- Test 1: Synchronous UI consistency for tab sign posting when attached and
// detached ---
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       TabSignPostingConsistency_AttachAndDetach) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Setup 3 tabs: Tab 0 active, Tab 1 and Tab 2 in background.
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Initial 0-tabs steady state across all elements.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 0),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 2.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Deselect Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// --- Test 2: Turn 2 with no new tabs retains existing context across all
// elements ---
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       MultiTurn_NoNewTabsAdded_PersistsAcrossTurns) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Turn 1: Attach Tab 1 and submit
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),

      // Turn 2: Submit follow-up without touching tabs
      SubmitSidePanelQuery(kSidePanelWebContentsId, "Second turn follow-up"),

      // Assert Tab 1 context persists across turns
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true));
}

// Starting a new thread clears all tabs submitted in the previous thread while
// showing the auto-suggested active tab on the new thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       NewThread_ClearsPreviousThreadTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const GURL kUrl3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),
      NavigateWebContents(kPrimaryTab, kUrl3),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),

      // Previous thread: Attach both background tabs in addition to the
      // auto-suggested active tab (title3) and submit a query.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 3),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "First+turn+query", "thread-1"),

      // Verify all 3 tabs remain active in the previous thread after
      // submission.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 3),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId,
                               {"title1", "title2", "title3"}),
      VerifyUnderlinedTabs({0, 1, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 3),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Transition to a new thread via the toolbar New Thread button.
      StartNewThreadFromSidePanel(kSidePanelWebContentsId),

      // Verify previous-thread tabs (title1 and title2) are cleared and only
      // the auto-suggested active tab (title3) is shown on the new thread.
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3"}),
      VerifyUnderlinedTabs({0}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false));
}

// Context uploads never complete, reproducing real network timing where the
// user switches to a tab in the window between attaching it and its upload
// finishing.
class ContextualTasksStalledUploadInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksStalledUploadInteractiveUiTest() = default;
  ~ContextualTasksStalledUploadInteractiveUiTest() override = default;

  UserVariation GetUserVariation() const override { return GetParam(); }
  bool ShouldStallContextUploads() const override { return true; }
};

IN_PROC_BROWSER_TEST_P(ContextualTasksStalledUploadInteractiveUiTest,
                       PanelFollowsTabAttachedWhileUploadInFlight) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kControlTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kAttachedTab, kUrl1),
      AddInstrumentedTab(kControlTab, kUrl2), SelectTab(kTabStripElementId, 0),
      OpenSidePanelWithWebContents(),

      // Attach Tab 1 via the tab picker. Its upload will never complete, so
      // the underline is the only signal that the attach has been processed.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      VerifyUnderlinedTabs({1}),

      CheckResult([this]() { return TaskIdForTab(0).is_valid(); }, true,
                  "Tab 0 should have a task once the panel is open"),
      CheckResult([this]() { return TaskIdForTab(1) == TaskIdForTab(0); }, true,
                  "Attached tab should share the task while upload is pending"),
      CheckResult([this]() { return TaskIdForTab(2) == TaskIdForTab(0); },
                  false, "CONTROL: an unattached tab must not share the task"),

      // Switching to the attached tab should carry the panel over.
      SelectTab(kTabStripElementId, 1),
      CheckResult(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          true, "Side panel should follow to a tab attached as context"));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksStalledUploadInteractiveUiTest,
                       TabDisassociatedWhenUploadFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kAttachedTab, kUrl1),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Attach Tab 1 via the tab picker while uploads are stalled.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      VerifyUnderlinedTabs({1}),

      CheckResult([this]() { return TaskIdForTab(0).is_valid(); }, true,
                  "Tab 0 should have a task once the panel is open"),
      CheckResult([this]() { return TaskIdForTab(1) == TaskIdForTab(0); }, true,
                  "Attached tab should share the task while upload is pending"),

      // Fail the in-flight upload and verify that Tab 1 is disassociated from
      // the task.
      Do([this]() { FailStalledUploads(); }),
      Do([this]() {
        EXPECT_TRUE(RunUntilNestable(
            [this]() { return !TaskIdForTab(1).is_valid(); }))
            << "Tab 1 should be disassociated from the task after upload fails";
      }));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       ContextPersistsAfterSwitchingToAttachedTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);

  const GURL kUrl0 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl1 = embedded_test_server()->GetURL("/title2.html");

  content::WebContents* panel_contents_before_switch = nullptr;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl0),
      AddInstrumentedTab(kAttachedTab, kUrl1), SelectTab(kTabStripElementId, 0),
      OpenSidePanelWithWebContents(),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({0}),

      // Attach Tab 1 as context from Tab 0's panel. Both Tab 0 and Tab 1
      // should now be in context in the side panel.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({0, 1}),

      Do([this, &panel_contents_before_switch]() {
        panel_contents_before_switch =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
      }),

      // Switch to the attached tab. It shares Tab 0's task, so the panel must
      // keep showing the very same WebContents and keep both Tab 0 and Tab 1
      // in context without requiring a query submission first.
      SelectTab(kTabStripElementId, 1),
      CheckResult(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          true, "Side panel should remain open on the attached tab"),
      CheckResult(
          [this, &panel_contents_before_switch]() {
            return ContextualTasksPanelController::From(browser())
                       ->GetActiveWebContents() == panel_contents_before_switch;
          },
          true, "Tabs in the same task must share one panel WebContents"),

      // Both Tab 0 and Tab 1 must still be reflected in the side panel context.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({0, 1}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// -----------------------------------------------------------------------------
// Omnibox-driven context management.
//
// Unlike the side panel, the omnibox composebox does NOT render the WebUI
// context menu (`cr-composebox-contextual-entrypoint-and-menu`). Clicking its
// "+" sends a `showContextMenu()` mojo call which opens a *native* Views menu
// built by `OmniboxContextMenuController`. The "Add Tabs" flyout is therefore a
// native submenu, and must be driven with Views steps (`SelectMenuItem`) rather
// than the DeepQuery helpers used above.
// -----------------------------------------------------------------------------
class ContextualTasksOmniboxContextManagementInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase {
 public:
  ContextualTasksOmniboxContextManagementInteractiveUiTest() = default;
  ~ContextualTasksOmniboxContextManagementInteractiveUiTest() override =
      default;

  void SetUpFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kContextualTasks, {}},
            {kContextualTasksForceEntryPointEligibility, {}},
            {omnibox::kContextManagementInComposebox,
             {{"enable_tab_deselection", "true"}}},
            // Gates the native "Add tabs" submenu. Without this
            // `AddRecentTabItems()` puts tabs directly in the main menu
            // instead, and `kSharedTabsSubmenuIdForTesting` is never attached.
            {omnibox::kContextManagementInOmnibox, {}},
            {omnibox::kTabFaviconChipsToCoins, {}},
            // Unlike the base fixture, the WebUI omnibox popups are the surface
            // under test here.
            {omnibox::internal::kWebUIOmniboxPopup, {}},
            {omnibox::internal::kWebUIOmniboxAimPopup, {}},
            {omnibox::internal::kWebUIOmniboxSimplification,
             {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
               "below_results"},
              {omnibox::kHideClassicContextButton.name, "false"}}},
            {omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady, {}},
            {omnibox::kAimEnabled, {}},
            {omnibox::kAimUsePecApi, {}},
            {lens::features::kLensOverlay, {}},
            {lens::features::kLensSidePanelUnification, {}},
            {lens::features::kLensOverlayContextualSearchbox, {}},
        },
        /*disabled_features=*/{
            // Keep `kOmniboxElementId` a Views element so focus and key presses
            // target it directly.
            features::kWebUILocationBar,
            // Eligibility is supplied by `MockAimEligibilityService`; don't let
            // the real service race it.
            omnibox::kAimServerEligibilityEnabled,
            omnibox::kAimFuseboxEligibilityCheckEnabled,
        });
  }

  void SetUpOnMainThread() override {
    ContextualTasksContextManagementInteractiveTestBase::SetUpOnMainThread();

    // The popup grows tall once the composebox is shown, and the native tabs
    // submenu is anchored to the right of the main menu. A default-sized window
    // can leave either off-screen, which makes clicks miss.
    browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 1280, 1024));

    // `OmniboxContextMenuController::AddRecentTabItems()` early-returns when
    // content sharing is disabled, so without this the "Add tabs" submenu is
    // never built.
    browser()->GetProfile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));
  }

 protected:
  // --- Omnibox popup DeepQuery paths ---
  // The classic popup's "+" context entrypoint.
  const DeepQuery kClassicContextEntrypoint = {
      "omnibox-popup-app", "omnibox-popup-contextual-entrypoint", "#context"};
  // The AIM popup composebox. Note this is `cr-omnibox-composebox`, a
  // different element from the side panel's `cr-composebox`; it renders the
  // bare `cr-composebox-contextual-entrypoint-button` rather than the WebUI
  // entrypoint-and-menu, so there is no `#entrypointButton` hop and no
  // `.share-tabs-flyout` here.
  const DeepQuery kAimComposebox = {"omnibox-aim-app", "#composebox"};
  const DeepQuery kAimContextEntrypoint = {"omnibox-aim-app", "#composebox",
                                           "#contextEntrypoint", "#entrypoint"};
  const DeepQuery kAimComposeboxInput = {"omnibox-aim-app", "#composebox",
                                         "cr-composebox-input", "#input"};
  const DeepQuery kAimSubmit = {"omnibox-aim-app", "#composebox",
                                "cr-composebox-submit", "#submitContainer"};

  // The browser-side omnibox model, which drives autocomplete and therefore
  // the popup.
  OmniboxEditModel* GetOmniboxEditModel() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBar()
        ->GetOmniboxController()
        ->edit_model();
  }

  // Focuses the omnibox and waits until the browser-side `OmniboxEditModel`
  // agrees that it is focused.
  auto FocusOmnibox() {
    return Steps(
        InAnyContext(WaitForShow(kOmniboxElementId)),
        InAnyContext(FocusElement(kOmniboxElementId)),
        PollState(kOmniboxFocusState,
                  [this]() { return GetOmniboxEditModel()->has_focus(); }),
        WaitForState(kOmniboxFocusState, true),
        StopObservingState(kOmniboxFocusState));
  }

  // An `OmniboxPopupView` may host multiple content views, but only one is
  // visible at a time; this returns the currently visible one.
  auto GetActiveClassicPopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      if (!browser_view || !browser_view->GetLocationBar()) {
        return nullptr;
      }
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          browser_view->GetLocationBar()->GetOmniboxPopupView());
      if (!popup_view || !popup_view->presenter()) {
        return nullptr;
      }
      return popup_view->presenter()->GetWebUIContent();
    });
  }

  auto GetActiveAimPopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      if (!browser_view || !browser_view->GetLocationBar()) {
        return nullptr;
      }
      auto* presenter_delegate =
          browser_view->GetLocationBar()->GetPresenterDelegate();
      if (!presenter_delegate) {
        return nullptr;
      }
      auto* aim_presenter = presenter_delegate->GetOmniboxPopupAimPresenter();
      if (!aim_presenter) {
        return nullptr;
      }
      return aim_presenter->GetWebUIContent();
    });
  }

  // `WebUiInteractiveTestMixin::WaitForElementToRender()` leaves
  // `continue_across_navigation` false. That is not safe for either omnibox
  // popup because both re-navigate while they are being set up.
  auto WaitForElementToRenderAcrossNavigation(ui::ElementIdentifier contents_id,
                                              const DeepQuery& where) {
    StateChange rendered;
    rendered.type = StateChange::Type::kExistsAndConditionTrue;
    rendered.where = where;
    rendered.test_function = R"(
        el => {
          const rect = el.getBoundingClientRect();
          return rect.width > 0 && rect.height > 0;
        })";
    rendered.event = kElementRenderedEvent;
    rendered.continue_across_navigation = true;
    return WaitForStateChange(contents_id, rendered);
  }

  // Opens the classic omnibox dropdown and instruments its `WebContents`.
  auto OpenOmniboxDropdown() {
    return Steps(
        FocusOmnibox(), EnterText(kOmniboxElementId, u"a"),
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kClassicPopupWebContentsId,
                                             GetActiveClassicPopupWebView())),
        InSameContext(
            WaitForWebContentsReady(kClassicPopupWebContentsId,
                                    GURL(chrome::kChromeUIOmniboxPopupURL))),
        InAnyContext(WaitForElementToRenderAcrossNavigation(
            kClassicPopupWebContentsId, kClassicContextEntrypoint)));
  }

  // Instruments the AIM popup, which replaces the classic popup once a tab is
  // selected from the context menu.
  auto WaitForAimPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kAimPopupWebContentsId,
                                             GetActiveAimPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kAimPopupWebContentsId, GURL(chrome::kChromeUIOmniboxPopupAimURL))),
        InAnyContext(WaitForElementToRenderAcrossNavigation(
            kAimPopupWebContentsId, kAimComposeboxInput)),
        InSameContext(ExecuteJsAt(kAimPopupWebContentsId, {}, R"(
          () => {
            const style = document.createElement('style');
            style.textContent =
                '* { animation: none !important; transition: none !important; }';
            document.head.appendChild(style);
          }
        )")));
  }

  auto ClickElementAcrossNavigation(ui::ElementIdentifier contents_id,
                                    const DeepQuery& where) {
    return Steps(WaitForElementToRenderAcrossNavigation(contents_id, where),
                 ScrollIntoView(contents_id, where),
                 MoveMouseTo(contents_id, where), ClickMouse());
  }

  // Clicks the classic popup's "+" and picks the current tab out of the native
  // "Add tabs" submenu.
  auto SelectCurrentTabFromOmniboxAddTabsMenu() {
    return Steps(
        InSameContext(ClickElementAcrossNavigation(kClassicPopupWebContentsId,
                                                   kClassicContextEntrypoint)),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)));
  }

  // Clicks the AIM composebox's "+" and picks the second tab out of the native
  // "Sharing 1 tab" / "Add tabs" submenu.
  //
  // This clicks via JS rather than synthesized mouse input on purpose. The AIM
  // popup keeps resizing after the composebox renders --
  // `OmniboxPopupWebUIBaseContent::ResizeDueToAutoResize()` debounces height
  // changes -- so a `MoveMouseTo()`/`ClickMouse()` pair can race the widget
  // moving out from under the cursor. The click then lands outside the popup
  // and dismisses it instead of opening the menu. `onEntrypointClick_()`
  // anchors the menu off `getBoundingClientRect()` rather than the event's
  // coordinates, so a JS click is equivalent minus the race.
  auto SelectSecondTabFromAimAddTabsMenu() {
    return Steps(
        InAnyContext(WaitForElementToRenderAcrossNavigation(
            kAimPopupWebContentsId, kAimContextEntrypoint)),
        InAnyContext(ExecuteJsAt(kAimPopupWebContentsId, kAimContextEntrypoint,
                                 "el => el.click()")),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kSecondTabMenuItemIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kSecondTabMenuItemIdForTesting)));
  }

  // Waits until the AIM composebox's "+" button shows the expected number of
  // favicon coins.
  auto WaitForAimComposeboxCoins(int expected_count) {
    StateChange coins_shown;
    coins_shown.type = StateChange::Type::kExistsAndConditionTrue;
    coins_shown.where = kAimComposebox;
    coins_shown.test_function = base::StringPrintf(
        R"(el => {
          const entrypoint =
              el.shadowRoot?.querySelector('#contextEntrypoint');
          const group =
              entrypoint?.shadowRoot?.querySelector('composebox-favicon-group');
          const coins =
              group?.shadowRoot?.querySelectorAll('.favicon-item') ?? [];
          return coins.length === %d;
        })",
        expected_count);
    coins_shown.event = kAimCoinsShownEvent;
    // The AIM popup re-navigates while the composebox is being set up, which
    // would otherwise silently invalidate this observer.
    coins_shown.continue_across_navigation = true;
    return InAnyContext(
        WaitForStateChange(kAimPopupWebContentsId, coins_shown));
  }

  // Types a query into the AIM composebox and submits it.
  auto SubmitAimComposeboxQuery(const std::string& query) {
    StateChange uploads_complete;
    uploads_complete.type = StateChange::Type::kExistsAndConditionTrue;
    uploads_complete.where = kAimComposebox;
    uploads_complete.test_function = "el => el && el.fileUploadsComplete";
    uploads_complete.event = kAimUploadsCompleteEvent;
    uploads_complete.continue_across_navigation = true;

    // Typing and the submit gate are a single poll: the composebox rebuilds
    // itself while the tab context settles, which can drop a value written on
    // an earlier tick. Re-writing it on every tick converges instead of racing.
    StateChange submit_enabled;
    submit_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    submit_enabled.where = kAimComposebox;
    submit_enabled.test_function = base::StringPrintf(
        R"(el => {
          if (!el) {
            return false;
          }
          if (!el.canSubmitFilesAndInput) {
            const host = el.shadowRoot?.querySelector('cr-composebox-input');
            const input = host?.shadowRoot?.querySelector('#input');
            if (!input) {
              return false;
            }
            input.value = %s;
            input.dispatchEvent(new Event('input', {bubbles: true}));
          }
          return el.canSubmitFilesAndInput;
        })",
        base::GetQuotedJSONString(query).c_str());
    submit_enabled.event = kAimSubmitEnabledEvent;
    submit_enabled.continue_across_navigation = true;

    // The first step must be `InAnyContext`: the popup lives in its own
    // `ui::ElementContext`, and the preceding step may have run in the browser
    // context (e.g. the `Do()`-based underline check).
    return Steps(
        InAnyContext(
            WaitForStateChange(kAimPopupWebContentsId, uploads_complete)),
        InSameContext(
            WaitForStateChange(kAimPopupWebContentsId, submit_enabled)),
        InSameContext(WaitForElementExists(kAimPopupWebContentsId, kAimSubmit)),
        InSameContext(
            ExecuteJsAt(kAimPopupWebContentsId, kAimSubmit, "el => el.click()")
                .SetMustRemainVisible(false)));
  }

  // Instruments the Contextual Tasks side panel that submitting opened, without
  // showing it itself. The side panel is back in the browser context, whereas
  // the preceding submit ran in the popup context.
  auto InstrumentOpenedSidePanel() {
    return Steps(
        InAnyContext(WaitForShow(kContextualTasksSidePanelWebViewElementId)),
        InSameContext(NameViewRelative(
            kContextualTasksSidePanelWebViewElementId,
            "SidePanelContentWebViewName",
            [](ContextualTasksWebView* web_view) -> views::View* {
              return web_view->content_web_view();
            })),
        InSameContext(InstrumentNonTabWebView(kSidePanelWebContentsId,
                                              "SidePanelContentWebViewName")),
        InSameContext(WaitForElementExists(kSidePanelWebContentsId,
                                           kComposeboxContainer)));
  }
};

// Adding the current tab from the omnibox opens the side panel with
// consistent sign posting.
// TODO(crbug.com/565713410): Re-enable this test.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && defined(MEMORY_SANITIZER)
#define MAYBE_OmniboxAddCurrentTab_OpensSidePanelWithSignposting \
  DISABLED_OmniboxAddCurrentTab_OpensSidePanelWithSignposting
#else
#define MAYBE_OmniboxAddCurrentTab_OpensSidePanelWithSignposting \
  OmniboxAddCurrentTab_OpensSidePanelWithSignposting
#endif
IN_PROC_BROWSER_TEST_F(
    ContextualTasksOmniboxContextManagementInteractiveUiTest,
    MAYBE_OmniboxAddCurrentTab_OpensSidePanelWithSignposting) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Tab 0 is the current tab. It must have a committed, shareable URL:
      // `GetRecentTabs()` filters on `IsValidTab()`, so an NTP or about:blank
      // tab would be dropped and no tab rows would be built. Tab 1 exists only
      // to prove exactly one tab gets signposted.
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl1),
      AddInstrumentedTab(kBackgroundTab1, kUrl2),
      SelectTab(kTabStripElementId, 0),

      // Nothing is shared before the user acts.
      VerifyUnderlinedTabs({}),

      // Open the omnibox dropdown and click the "+" in it.
      // Pick the current tab out of the native "Add Tabs" submenu.
      OpenOmniboxDropdown(), SelectCurrentTabFromOmniboxAddTabsMenu(),

      // Selecting a tab hands the classic popup off to the AIM composebox,
      // which should come up already carrying the current tab.
      InAnyContext(WaitForHide(kClassicPopupWebContentsId)),
      WaitForAimPopupReady(), WaitForAimComposeboxCoins(1),
      VerifyUnderlinedTabs({0}),

      // Submitting with the current tab in context opens the side panel.
      SubmitAimComposeboxQuery("What is on this page?"),
      InstrumentOpenedSidePanel(),

      // The context carries over, and the side panel signposts it the same
      // way: tab 0 still underlined, and a single favicon coin on the side
      // panel composebox's "+" button.
      //
      // TODO(crbug.com/564561893): Also assert the tab is checked in the side
      // panel's Add Tabs flyout.
      VerifyUnderlinedTabs({0}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1));
}

// Adding the current tab plus another tab via the omnibox context menu always
// opens the Contextual Tasks side panel with both tabs in context.
IN_PROC_BROWSER_TEST_F(
    ContextualTasksOmniboxContextManagementInteractiveUiTest,
    OmniboxAddCurrentTabAndAnotherTab_OpensSidePanelWithBothTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const GURL kUrl3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl1),
      AddInstrumentedTab(kBackgroundTab1, kUrl2),
      AddInstrumentedTab(kBackgroundTab2, kUrl3),
      SelectTab(kTabStripElementId, 0),

      VerifyUnderlinedTabs({}),

      // Select the current tab (Tab 0) from the classic omnibox "+" menu.
      OpenOmniboxDropdown(), SelectCurrentTabFromOmniboxAddTabsMenu(),
      InAnyContext(WaitForHide(kClassicPopupWebContentsId)),
      WaitForAimPopupReady(), WaitForAimComposeboxCoins(1),
      VerifyUnderlinedTabs({0}),

      // Now select a second tab from the AIM composebox's "+" menu.
      // `GetRecentTabs()` orders non-active tabs by `last_active` descending,
      // so `kSecondTabMenuItemIdForTesting` corresponds to Tab 2 (while Tab 1
      // remains unshared as a control).
      SelectSecondTabFromAimAddTabsMenu(), WaitForAimComposeboxCoins(2),
      VerifyUnderlinedTabs({0, 2}),

      // Submitting with the current tab + another tab in context must open the
      // Contextual Tasks side panel with both tabs preserved.
      SubmitAimComposeboxQuery("Compare these two pages"),
      InstrumentOpenedSidePanel(),

      VerifyUnderlinedTabs({0, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksContextManagementInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksStalledUploadInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

}  // namespace contextual_tasks
