// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MANAGEMENT_INTERACTIVE_TEST_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MANAGEMENT_INTERACTIVE_TEST_BASE_H_

#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/webview/webview.h"

namespace contextual_tasks {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kMenuOpenEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kFlyoutVisibleEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kCoinsReadyEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kTriggerReadyEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kCheckedReadyEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSubmitEnabledEvent);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kUploadsCompleteEvent);

class ContextualTasksContextManagementInteractiveTestBase
    : public ContextualTasksInteractiveTestBase {
 public:
  using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

  // `composebox_root` is the DeepQuery path to the composebox element hosting
  // the shared `cr-composebox-contextual-entrypoint-and-menu`. It defaults to
  // the side panel composebox; other surfaces that render the same component
  // (e.g. the NTP realbox composebox) pass their own root so the sign-posting
  // helpers below apply unchanged.
  explicit ContextualTasksContextManagementInteractiveTestBase(
      DeepQuery composebox_root = {"contextual-tasks-app", "#composebox",
                                   "#composebox"});
  ~ContextualTasksContextManagementInteractiveTestBase() override;

  void SetUpFeatureList() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  // --- DeepQuery Paths ---
  // Paths rooted at the composebox passed to the constructor. Everything
  // under `#contextEntrypoint` belongs to the shared
  // `cr-composebox-contextual-entrypoint-and-menu` component.
  const DeepQuery kComposeboxContainer;
  const DeepQuery kContextEntrypoint;
  const DeepQuery kContextMenu;
  const DeepQuery kShareTabsTrigger;
  const DeepQuery kShareTabsFlyout;

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

  // --- Kombucha Custom Verbs ---

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
  // step.
  bool RunUntilNestable(base::FunctionRef<bool()> condition);

  // Asynchronously verifies whether the native tab strip reflects the expected
  // set of underlined tabs.
  auto VerifyUnderlinedTabs(const std::set<int>& expected_indices) {
    return CheckResult(
        [this, expected_indices]() {
          return WaitForUnderlinedTabsToMatch(expected_indices);
        },
        true,
        "Tab strip underlines did not match the expected set of indices.");
  }

  // Opens the share tabs flyout menu inside the composebox.
  // Note: Direct mutation of hover/timer properties (e.g. pointerOverFlyout_,
  // cancelCloseTimer_) is intentionally used here to bypass asynchronous hover
  // delay timers in the WebUI and prevent flakiness under test runners.
  auto OpenShareTabsFlyout(const ui::ElementIdentifier& contents_id) {
    StateChange menu_open;
    menu_open.type = StateChange::Type::kExistsAndConditionTrue;
    menu_open.where = kShareTabsTrigger;
    menu_open.test_function = "el => !!el.getRootNode().host?.open";
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

  // Shows the Contextual Tasks side panel with kOmniboxPageAction invocation
  // source. Setting kOmniboxPageAction ensures that
  // ContextualTasksUI::ShouldClearAllInputsOnSubmit evaluates to false,
  // preventing submitted tab tokens and input context from being dropped across
  // turns in multi-turn flows.
  auto ShowSidePanel() {
    return Do([this]() { ShowSidePanelForTesting(); });
  }

  // Instruments the Contextual Tasks side panel after it is shown. Uses
  // `InAnyContext` because callers may invoke this after a step that ran in a
  // popup context (e.g. the omnibox AIM popup).
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

  // Shows the Contextual Tasks side panel and instruments its WebContents.
  auto OpenSidePanelWithWebContents() {
    return Steps(ShowSidePanel(), InstrumentOpenedSidePanel());
  }

  // Waits for a full-tab Contextual Tasks navigation
  // (`chrome://contextual-tasks/`) and its embedded `<webview>`
  // (`#threadFrame`) to finish initializing, including
  // `ContextualTasksUI::CreatePageHandler` and `FrameNavObserver`'s
  // `window.history.replaceState` URL sync. Navigating away before the initial
  // `<webview>` load and `replaceState` settle causes `NavigateWebContents` to
  // observe the in-flight same-document `replaceState` URL change instead of
  // the new navigation.
  auto WaitForFullTabContextualTasksLoaded(
      const ui::ElementIdentifier& contents_id) {
    StateChange initial_load_done;
    initial_load_done.type = StateChange::Type::kExistsAndConditionTrue;
    initial_load_done.where = {"contextual-tasks-app"};
    initial_load_done.test_function =
        "el => el.hasFinishedTopLevelNavigationForTesting_ === true && "
        "!el.isFrameLoading && !el.pendingUrl_";
    initial_load_done.event = kElementExistsEvent;
    initial_load_done.continue_across_navigation = true;

    return Steps(
        WaitForWebContentsNavigation(contents_id),
        CheckElement(
            contents_id,
            [](ui::TrackedElement* el) {
              return AsInstrumentedWebContents(el)
                  ->web_contents()
                  ->GetLastCommittedURL()
                  .host();
            },
            std::string(chrome::kChromeUIContextualTasksHost)),
        InstrumentInnerWebContents(kInnerWebContentsId, contents_id, 0),
        WaitForStateChange(contents_id, initial_load_done),
        UninstrumentWebContents(kInnerWebContentsId));
  }

  // Returns the task ID that the tab at `index` is associated with in
  // ContextualTasksService, or an invalid Uuid if it has no task. This is the
  // association that drives whether the side panel follows the user to a tab;
  // it is distinct from the tab-strip underline, which reflects the session's
  // uploaded context attachments.
  base::Uuid TaskIdForTab(int index);

  // When true, context uploads are held open indefinitely instead of
  // completing immediately. The production interceptor answers uploads
  // synchronously, which hides any behavior that depends on a tab being
  // attached while its upload is still in flight.
  virtual bool ShouldStallContextUploads() const;

  void FailStalledUploads();

 private:
  bool WaitForUnderlinedTabsToMatch(const std::set<int>& expected_indices);
  void ShowSidePanelForTesting();

  bool fail_context_uploads_ = false;
  std::vector<mojo::Remote<network::mojom::URLLoaderClient>>
      stalled_upload_clients_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MANAGEMENT_INTERACTIVE_TEST_BASE_H_
