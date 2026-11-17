// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/auto_reset.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_management_interactive_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace contextual_tasks {

namespace {

// WebContents of the two WebUI omnibox popup surfaces. The classic popup hosts
// the suggestion list plus the context entrypoint; selecting a tab from its
// context menu swaps it for the AIM popup, which hosts the composebox.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAimPopupWebContentsId);
// Whether the browser-side omnibox has focus; autocomplete only runs while it
// does, so the popup will not open until this is true.
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kOmniboxFocusState);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRenderedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimSubmitEnabledEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimUploadsCompleteEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimCoinsShownEvent);

}  // namespace

// -----------------------------------------------------------------------------
// Omnibox-driven context management.
//
// Unlike the side panel, the omnibox composebox does NOT render the WebUI
// context menu (`cr-composebox-contextual-entrypoint-and-menu`). Clicking its
// "+" sends a `showContextMenu()` mojo call which opens a *native* Views menu
// built by `OmniboxContextMenuController`. The "Add Tabs" flyout is therefore a
// native submenu, and must be driven with Views steps (`SelectMenuItem`) rather
// than the DeepQuery helpers in the base class.
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
            {lens::features::kLensOverlayContextualSearchbox, {}},
        },
        /*disabled_features=*/{
            kContextualTasksRearchitecture,
            kContextualTasksSidePanelRearchitecture,
            // Keep `kOmniboxElementId` a Views element so focus and key presses
            // target it directly.
            features::kWebUILocationBar,
            // Eligibility is supplied by `MockAimEligibilityService`; don't let
            // the real service race it.
            omnibox::kAimServerEligibilityEnabled,
            omnibox::kAimFuseboxEligibilityCheckEnabled,
            omnibox::kComposeboxPersistentAimButtonOmnibox,
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
  views::View* GetActiveClassicPopupWebView() {
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
  }

  views::View* GetActiveAimPopupWebView() {
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
        InSameContext(NameViewRelative(
            OmniboxPopupPresenterBase::kRoundedResultsFrame,
            "ClassicPopupWebView",
            [this](views::View*) { return GetActiveClassicPopupWebView(); })),
        InSameContext(InstrumentNonTabWebView(kClassicPopupWebContentsId,
                                              "ClassicPopupWebView")),
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
        InSameContext(NameViewRelative(
            OmniboxPopupPresenterBase::kRoundedResultsFrame, "AimPopupWebView",
            [this](views::View*) { return GetActiveAimPopupWebView(); })),
        InSameContext(
            InstrumentNonTabWebView(kAimPopupWebContentsId, "AimPopupWebView")),
        InSameContext(WaitForWebContentsReady(
            kAimPopupWebContentsId, GURL(chrome::kChromeUIOmniboxPopupAimURL))),
        InAnyContext(WaitForElementToRenderAcrossNavigation(
            kAimPopupWebContentsId, kAimComposeboxInput)),
        InSameContext(ExecuteJsAt(kAimPopupWebContentsId, {}, R"(
          () => {
            const style = document.createElement('style');
            style.textContent = '* { animation: none !important; ' +
                'transition: none !important; }';
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

  // Clicks the classic popup's "+" and picks the second (non-current) tab out
  // of the native "Add tabs" submenu.
  auto SelectSecondTabFromOmniboxAddTabsMenu() {
    return Steps(
        InSameContext(ClickElementAcrossNavigation(kClassicPopupWebContentsId,
                                                   kClassicContextEntrypoint)),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting)),
        InAnyContext(WaitForShow(
            OmniboxContextMenuController::kSecondTabMenuItemIdForTesting)),
        InSameContext(SelectMenuItem(
            OmniboxContextMenuController::kSecondTabMenuItemIdForTesting)));
  }

  // Opens the AIM composebox's "+" context menu and expands the "Add tabs" /
  // "Sharing n tabs" submenu without selecting a tab item.
  auto OpenAimAddTabsSubmenu() {
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
            OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)));
  }

  // Verifies the Views submenu trigger label ("Add tabs" vs "Sharing n tabs").
  auto VerifyOmniboxSharedTabsSubmenuLabel(int expected_shared_count) {
    const std::u16string expected_label =
        expected_shared_count > 0
            ? l10n_util::GetPluralStringFUTF16(IDS_COMPOSE_SHARING_TABS,
                                               expected_shared_count)
            : l10n_util::GetStringUTF16(IDS_COMPOSE_ADD_TABS);
    return InAnyContext(CheckViewProperty(
        OmniboxContextMenuController::kSharedTabsSubmenuIdForTesting,
        &views::MenuItemView::title, expected_label));
  }

  // Verifies whether a tab row in the Views submenu has a checkmark minor icon.
  auto VerifyOmniboxTabMenuItemChecked(ui::ElementIdentifier item_id,
                                       bool expected_checked) {
    return InAnyContext(CheckView(
        item_id,
        [](views::MenuItemView* item) {
          return !item->GetMinorIcon().IsEmpty();
        },
        expected_checked));
  }

  // Clicks the AIM composebox's "+" and picks the first tab out of the native
  // "Sharing n tabs" / "Add tabs" submenu (used to deselect or re-select it).
  auto SelectFirstTabFromAimAddTabsMenu() {
    return Steps(
        OpenAimAddTabsSubmenu(),
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
        OpenAimAddTabsSubmenu(),
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
    // context (e.g. the underline check).
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

 private:
  // TODO(crbug.com/542347163): These tests fail when the omnibox popups create
  // their WebContents only when shown, which is the default in browser tests
  // (see `InProcessBrowserTest`). Adding a tab from the classic popup's context
  // menu creates the AI Mode popup's page then, and the tab context seems to be
  // lost while that page loads. Either keep that context until the page is
  // ready or create the page explicitly where it is needed, and remove this
  // override.
  base::AutoReset<bool> create_popup_webui_on_demand_ =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(false);
};

// Adding the current tab from the omnibox opens the side panel with
// consistent sign posting.
IN_PROC_BROWSER_TEST_F(ContextualTasksOmniboxContextManagementInteractiveUiTest,
                       OmniboxAddCurrentTab_OpensSidePanelWithSignposting) {
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

      // The Views context menu reflects "Sharing 1 tab" and a checkmark on the
      // current tab row, while the unshared tab row has no checkmark.
      OpenAimAddTabsSubmenu(), VerifyOmniboxSharedTabsSubmenuLabel(1),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kFirstTabMenuItemIdForTesting, true),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kSecondTabMenuItemIdForTesting, false),

      // Submitting with the current tab in context opens the side panel.
      SubmitAimComposeboxQuery("What is on this page?"),
      InstrumentOpenedSidePanel(),

      // The context carries over, and the side panel signposts it the same
      // way: tab 0 still underlined and a single favicon coin on the side
      // panel composebox's "+" button.
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

      // Verify the Views context menu reflects "Sharing 2 tabs" with
      // checkmarks on both selected tabs.
      OpenAimAddTabsSubmenu(), VerifyOmniboxSharedTabsSubmenuLabel(2),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kFirstTabMenuItemIdForTesting, true),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kSecondTabMenuItemIdForTesting, true),

      // Submitting with the current tab + another tab in context must open the
      // Contextual Tasks side panel with both tabs preserved.
      SubmitAimComposeboxQuery("Compare these two pages"),
      InstrumentOpenedSidePanel(),

      VerifyUnderlinedTabs({0, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2));
}

// Deselecting a tab from the AIM popup's "+" context menu removes its coin,
// tab-strip underline, and Views menu checkmark, returning the submenu label
// to "Add tabs".
IN_PROC_BROWSER_TEST_F(ContextualTasksOmniboxContextManagementInteractiveUiTest,
                       OmniboxAimPopup_DeselectTab_ClearsSignposting) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl1),
      AddInstrumentedTab(kBackgroundTab1, kUrl2),
      SelectTab(kTabStripElementId, 0), VerifyUnderlinedTabs({}),

      // Select Tab 0 from the classic omnibox "+" menu to enter the AIM popup.
      OpenOmniboxDropdown(), SelectCurrentTabFromOmniboxAddTabsMenu(),
      InAnyContext(WaitForHide(kClassicPopupWebContentsId)),
      WaitForAimPopupReady(), WaitForAimComposeboxCoins(1),
      VerifyUnderlinedTabs({0}),

      // Open the AIM "+" submenu and verify the Views menu reflects 1 shared
      // tab with a checkmark on the first tab item, then click to deselect it.
      OpenAimAddTabsSubmenu(), VerifyOmniboxSharedTabsSubmenuLabel(1),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kFirstTabMenuItemIdForTesting, true),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kSecondTabMenuItemIdForTesting, false),
      InSameContext(SelectMenuItem(
          OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)),

      // Verify coin and tab-strip underline are removed, and the Views menu
      // returns to "Add tabs" with no checkmarks.
      WaitForAimComposeboxCoins(0), VerifyUnderlinedTabs({}),
      OpenAimAddTabsSubmenu(), VerifyOmniboxSharedTabsSubmenuLabel(0),
      VerifyOmniboxTabMenuItemChecked(
          OmniboxContextMenuController::kFirstTabMenuItemIdForTesting, false));
}

// Selecting only a non-current (background) tab from the omnibox "+" menu
// fulfills in the active tab (full-page Contextual Tasks UI) rather than the
// side panel, keeps the background tab underlined while on the AIM page, and
// clears the underline when the user navigates the active tab away.
IN_PROC_BROWSER_TEST_F(
    ContextualTasksOmniboxContextManagementInteractiveUiTest,
    OmniboxAddNonCurrentTab_FulfillsInAimFullPageAndCleansUpOnExit) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl1),
      AddInstrumentedTab(kBackgroundTab1, kUrl2),
      SelectTab(kTabStripElementId, 0), VerifyUnderlinedTabs({}),

      // Select only the background tab (Tab 1) from the classic omnibox "+"
      // menu while Tab 0 is active.
      OpenOmniboxDropdown(), SelectSecondTabFromOmniboxAddTabsMenu(),
      InAnyContext(WaitForHide(kClassicPopupWebContentsId)),
      WaitForAimPopupReady(), WaitForAimComposeboxCoins(1),
      VerifyUnderlinedTabs({1}),

      // Submit the query. Because the active tab (Tab 0) is not in context,
      // `ContextualSearchboxHandler::OpenUrl` navigates Tab 0 to the AIM full
      // page (`chrome://contextual-tasks/`) rather than opening the side panel.
      SubmitAimComposeboxQuery("Summarize background tab"),
      WaitForFullTabContextualTasksLoaded(kPrimaryTab),
      VerifyUnderlinedTabs({1}),

      // Navigating Tab 0 away from the AIM page clears the task session and
      // removes Tab 1's underline.
      NavigateWebContents(kPrimaryTab, kUrl1), VerifyUnderlinedTabs({}));
}

}  // namespace contextual_tasks
