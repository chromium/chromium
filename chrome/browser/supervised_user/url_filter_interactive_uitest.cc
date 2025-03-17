// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/browser_user.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/family_link_settings_state_management.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/event_modifiers.h"
#include "ui/views/widget/widget_utils.h"

namespace supervised_user {
namespace {

static constexpr std::string_view kPermissionRequestUrl =
    "https://families.google.com/u/0/manage/family/";

// Aria label of the approval button, customizable in the PACP soy resources.
static constexpr std::string_view kLocalApprovalButtonAriaLabel = "Approve";
// Password selector for the parent password field in the PACP dialog.
static constexpr std::string_view kPacpPasswordInputSelector = "type=password";

// All tests in this unit are subject to flakiness because they interact with a
// system that can be externally modified during execution.
// TODO(b/301587955): Fix placement of supervised_user/e2e test files and their
// dependencies.
class UrlFilterUiTest
    : public InteractiveFamilyLiveTest,
      public testing::WithParamInterface<FamilyLiveTest::RpcMode> {
 public:
  UrlFilterUiTest()
      : InteractiveFamilyLiveTest(
            GetParam(),
            /*extra_enabled_hosts=*/{"example.com", "bestgore.com"}) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {supervised_user::kUncredentialedFilteringFallbackForSupervisedUsers,
         supervised_user::kLocalWebApprovals},
        /*disabled_features=*/{});
#endif // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  }

 protected:
  auto ParentOpensControlListPage(ui::ElementIdentifier kParentTab,
                                  const GURL& gurl) {
    return Steps(NavigateWebContents(kParentTab, gurl),
                 WaitForWebContentsReady(kParentTab, gurl));
  }

  auto ParentRemovesUrlsFromControlList(ui::ElementIdentifier kParentTab) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFiltersAreRemoved);

    StateChange all_url_filters_removed;
    all_url_filters_removed.type = StateChange::Type::kDoesNotExist;
    all_url_filters_removed.where = {"#view_container li"};
    all_url_filters_removed.event = kFiltersAreRemoved;

    return Steps(ExecuteJsAt(kParentTab, {"#view_container"}, R"js(
          view_container => {
            // Clicks remove on all filters.
            for(const li of view_container.querySelectorAll("li")) {
                li.children[1].children[0].click();
            }
          }
        )js"),
                 WaitForStateChange(kParentTab, all_url_filters_removed));
  }

  StateChange RemoteApprovalButtonAppeared() {
    return ElementHasAppeared({"#frame-blocked #remote-approvals-button"});
  }

  StateChange LocalApprovalButtonAppeared() {
    return ElementHasAppeared({"#frame-blocked #local-approvals-button"});
  }

  StateChange ReauthenticationInterstitialNextButtonAppeared() {
    return ElementHasAppeared({".supervised-user-verify #primary-button"});
  }

  StateChange SignInButtonsAppeared() {
    return ElementHasAppeared({"#identifierNext"});
  }

  StateChange ParentPasswordEntryAppeared() {
    return ElementHasAppeared({"#password"});
  }

  StateChange PacpApprovalButtonAppeared() {
    return ElementHasAppeared({base::StringPrintf(
        "button[aria-label='%s']", kLocalApprovalButtonAriaLabel)});
  }

  StateChange PacpPasswordFieldAppeared() {
    return ElementHasAppeared({"[type='password']"});
  }

  auto WaitForPacpDialogToAppear(ui::ElementIdentifier kPacpDialogId) {
    return Steps(
        WaitForStateChange(kPacpDialogId, PacpApprovalButtonAppeared()),
        WaitForStateChange(kPacpDialogId, PacpPasswordFieldAppeared()));
  }

  // Clicks the approval request button for a pending request on Family Link.
  auto ParentApprovesPermissionRequest(ui::ElementIdentifier kParentTab) {
    return Steps(ExecuteJsAt(
        kParentTab, {"#view_container"},
        // The "Allow All" is the last button on the permission request page.
        R"js(
                (view_container) => {
                  const buttons = view_container.querySelectorAll("div[role='button']");
                  const allow = buttons[buttons.length - 1];
                  allow.click();
                }
              )js"));
  }

  // Clicks the remote approval request button on the supervised user
  // interstitial.
  auto ChildRequestsRemoteApproval(ui::ElementIdentifier kChildTab) {
    return Steps(ExecuteJsAt(kChildTab,
                             {"#frame-blocked #remote-approvals-button"},
                             R"js( (button) => { button.click(); } )js"));
  }

  // Clicks the local approval request button on the supervised user
  // interstitial.
  auto ChildRequestsLocalApproval(ui::ElementIdentifier kChildTab) {
    return Steps(ExecuteJsAt(kChildTab,
                             {"#frame-blocked #local-approvals-button"},
                             R"js( (button) => { button.click(); } )js"));
  }

  // Clicks the 'Next' button on the supervised user re-authentication
  // interstitial.
  auto ChildProceedsToSignIn(ui::ElementIdentifier kChildTab) {
    return Steps(ExecuteJsAt(kChildTab,
                             {".supervised-user-verify #primary-button"},
                             R"js( (button) => { button.click(); } )js"));
  }

  // Clicks the approval button on the PACP dialog.
  auto UserClicksPacpApprovalButton(ui::ElementIdentifier kPacpDialogId) {
    return Steps(
        ExecuteJsAt(kPacpDialogId,
                    {base::StringPrintf("button[aria-label='%s']",
                                        kLocalApprovalButtonAriaLabel)},
                    R"js( (approve_button) => { approve_button.click(); } )js",
                    ExecuteJsMode::kFireAndForget));
  }

  // Performs a child sign-in from the UI that is opened by the
  // Re-authentication interstitial.
  auto DoChildSignInFromUI(ui::ElementIdentifier kChildSignInElementId) {
    return Steps(
        Log("When sign-in first page is loaded"),
        WaitForStateChange(kChildSignInElementId, SignInButtonsAppeared()),
        Log("Child proceeds to Next sign-in page"),
        // On the first sign-in page click the "Next" button to be presented
        // with the prompt for credentials.
        ExecuteJsAt(kChildSignInElementId, {"#identifierNext > div > button"},
                    R"js( (button) => { button.click(); } )js"),
        // Confirm the password entry field appears.
        WaitForStateChange(kChildSignInElementId,
                           ParentPasswordEntryAppeared()),
        Log("Sign-in page is ready"),
        // Fill-in the password field.
        ExecuteJsAt(kChildSignInElementId,
                    {base::StringPrintf("#password input[%s]",
                                        kPacpPasswordInputSelector)},
                    base::StringPrintf(
                        R"js( (entry) => { entry.value = "%s"; } )js",
                        std::string(child().GetAccountPassword()).c_str())),
        // Click the "Next" button which concludes the sign-in.
        ExecuteJsAt(kChildSignInElementId, {"#passwordNext > div > button"},
                    R"js( (button) => { button.click(); } )js"),
        Log("Child fills in password and proceeds"));
  }

  // Checks that a permission request exists on Family link.
  StateChange RemotePermissionRequestAppeared() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = {"#view_container"};

    // The "Allow All" (host approval) is the last button on the permission
    // request page.
    std::string_view remote_permission_approval_button_text = "ALLOW ALL";
    state_change.test_function =
        base::StringPrintf(R"js(
          (view_container) => {
            const buttons = view_container.querySelectorAll("div[role='button']");
            return buttons[buttons.length - 1].innerText == '%s'; }
          )js",
                           remote_permission_approval_button_text.data());
    state_change.event = kTextFound;
    state_change.continue_across_navigation = true;
    return state_change;
  }

  // Checks if a page title matches the given regexp in ecma script dialect.
  StateChange PageWithMatchingTitle(std::string_view title_regexp) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kStateChange;
    state_change.test_function = base::StringPrintf(R"js(
      () => /%s/.test(document.title)
    )js",
                                                    title_regexp.data());
    state_change.continue_across_navigation = true;
    return state_change;
  }

 private:
  StateChange ElementHasAppeared(DeepQuery element_selector) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kExists;
    state_change.where = element_selector;
    state_change.event = kStateChange;
    state_change.continue_across_navigation = true;
    return state_change;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ParentBlocksPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kSetSafeSitesStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);

  TurnOnSync();

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL all_audiences_site_url(GetRoutedUrl("https://example.com"));

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, child(),
                          FamilyLinkSettingsState::Reset()),
      WaitForStateSeeding(kSetSafeSitesStateObserverId, child(),
                          FamilyLinkSettingsState::EnableSafeSites()),

      // Supervised user navigates to any page.
      InstrumentTab(kChildElementId, tab_index, &child().browser()),
      NavigateWebContents(kChildElementId, all_audiences_site_url),
      WaitForStateChange(kChildElementId,
                         PageWithMatchingTitle("Example Domain")),
      // Supervisor blocks that page and supervised user sees interstitial
      // blocked page screen.
      WaitForStateSeeding(
          kDefineStateObserverId, child(),
          FamilyLinkSettingsState::BlockSite(all_audiences_site_url)),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()));
}

// Sanity test, if it fails it means that resetting the test state is not
// functioning properly.
IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ClearFamilyLinkSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver, kObserverId);

  TurnOnSync();

  // Clear all existing filters.
  RunTestSequence(WaitForStateSeeding(kObserverId, child(),
                                      FamilyLinkSettingsState::Reset()));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ParentAllowsPageBlockedBySafeSites) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);

  TurnOnSync();

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL mature_site_url(GetRoutedUrl("https://bestgore.com"));

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, child(),
                          FamilyLinkSettingsState::Reset()),

      // Supervised user navigates to inappropriate page and is blocked.
      InstrumentTab(kChildElementId, tab_index, &child().browser()),
      NavigateWebContents(kChildElementId, mature_site_url),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),

      // Supervisor allows that page and supervised user consumes content.
      WaitForStateSeeding(kDefineStateObserverId, child(),
                          FamilyLinkSettingsState::AllowSite(mature_site_url)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Best Gore")));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest,
                       ParentApprovesPermissionRequestForBlockedSite) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentApprovalTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);

  TurnOnSync();

  // Child and parent activity is happening in these tabs.
  int child_tab_index = 0;
  int parent_tab_index = 0;

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, child(),
                          FamilyLinkSettingsState::Reset()),
      // Supervised user navigates to inappropriate page and is blocked, and
      // makes approval request.
      InstrumentTab(kChildElementId, child_tab_index, &child().browser()),
      Log("When child navigates to blocked url"),
      NavigateWebContents(kChildElementId,
                          GetRoutedUrl("https://bestgore.com")),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),
      Log("When child requests approval"),
      ChildRequestsRemoteApproval(kChildElementId),

      // Parent receives remote approval request for the blocked page in Family
      // Link.
      Log("When parent receives approval request"),
      InstrumentTab(kParentApprovalTab, parent_tab_index,
                    /*in_browser=*/&head_of_household().browser()),
      ParentOpensControlListPage(kParentApprovalTab,
                                 GURL(kPermissionRequestUrl)),
      WaitForStateChange(kParentApprovalTab, RemotePermissionRequestAppeared()),

      // Parent approves the request and supervised user consumes the page
      // content.
      Log("When parent grants approval"),
      ParentApprovesPermissionRequest(kParentApprovalTab),
      Log("Then child gets unblocked"),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Best Gore")));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, DesktopLocalWebApprovalGranted) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPacpViewElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);

  base::HistogramTester histogram_tester;

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL mature_site_url(GetRoutedUrl("https://bestgore.com"));

  TurnOnSync();
  RunTestSequence(InAnyContext(Steps(
      WaitForStateSeeding(kResetStateObserverId, child(),
                          FamilyLinkSettingsState::Reset()),
      // Supervised user navigates to inappropriate page and is blocked.
      InstrumentTab(kChildElementId, tab_index, &child().browser()),
      NavigateWebContents(kChildElementId, mature_site_url),
      Log("When child is shown the interstitial"),
      // The user clicks the local approval button.
      WaitForStateChange(kChildElementId, LocalApprovalButtonAppeared()),
      ChildRequestsLocalApproval(kChildElementId),
      Log("When child requests local web approval"),
      // The PACP dialog appears.
      WaitForShow(kLocalWebParentApprovalDialogId),
      InstrumentNonTabWebView(kPacpViewElementId,
                              kLocalWebParentApprovalDialogId),
      WaitForPacpDialogToAppear(kPacpViewElementId),
      Log("When parent approval dialog opens"),
      // The parent provides their password.
      ExecuteJsAt(
          kPacpViewElementId,
          {base::StringPrintf("[%s]", kPacpPasswordInputSelector)},
          base::StringPrintf(
              R"js( (entry) => { entry.value = "%s"; } )js",
              std::string(head_of_household().GetAccountPassword()).c_str())),
      // The parent clicks the approval button on the dialog.
      UserClicksPacpApprovalButton(kPacpViewElementId),
      Log("When parent approves the request in the dialog"),
      WaitForHide(kLocalWebParentApprovalDialogId),
      Log("Then the parent approval dialog closes"),
      // The page gets unblocked.
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Best Gore")),
      Log("Then the child gets unblocked"), Do([&]() {
        histogram_tester.ExpectBucketCount(
            "FamilyLinkUser.LocalWebApprovalResult",
            supervised_user::LocalApprovalResult::kApproved, 1);
        histogram_tester.ExpectTotalCount(
            "FamilyLinkUser.LocalWebApprovalResult", 1);
      }),
      Log("Then the approval metrics are recorded"))));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest,
                       DesktopLocalWebApprovalCancelledViaCloseButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPacpViewElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);
  base::HistogramTester histogram_tester;

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL mature_site_url(GetRoutedUrl("https://bestgore.com"));

  TurnOnSync();

  RunTestSequence(InAnyContext(
      Steps(WaitForStateSeeding(kResetStateObserverId, child(),
                                FamilyLinkSettingsState::Reset()),
            // Supervised user navigates to inappropriate page and is blocked.
            Log("When child is shown the interstitial"),
            InstrumentTab(kChildElementId, tab_index, &child().browser()),
            NavigateWebContents(kChildElementId, mature_site_url),
            // The user clicks the local approval button.
            WaitForStateChange(kChildElementId, LocalApprovalButtonAppeared()),
            Log("When child requests local web approval"),
            ChildRequestsLocalApproval(kChildElementId),
            // The PACP dialog appears.
            Log("When the PACP dialog shows up"),
            WaitForShow(kLocalWebParentApprovalDialogId),
            InstrumentNonTabWebView(kPacpViewElementId,
                                    kLocalWebParentApprovalDialogId),
            WaitForPacpDialogToAppear(kPacpViewElementId),
            // The PACP dialog is dismissed via the "X" close button.
            Log("When the user clicks the Close button"),
            PressButton(views::BubbleFrameView::kCloseButtonElementId),
            WaitForHide(kPacpViewElementId),
            Log("Then the dialog is dismissed and a cancellation is recorded"),
            Do([&]() {
              histogram_tester.ExpectBucketCount(
                  "FamilyLinkUser.LocalWebApprovalResult",
                  supervised_user::LocalApprovalResult::kCanceled, 1);
              histogram_tester.ExpectTotalCount(
                  "FamilyLinkUser.LocalWebApprovalResult", 1);
            }))));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest,
                       DesktopLocalWebApprovalCancelledViaAccelerator) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPacpViewElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);
  base::HistogramTester histogram_tester;

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL mature_site_url(GetRoutedUrl("https://bestgore.com"));

  TurnOnSync();
  RunTestSequence(InAnyContext(Steps(
      WaitForStateSeeding(kResetStateObserverId, child(),
                          FamilyLinkSettingsState::Reset()),
      // Supervised user navigates to inappropriate page and is blocked.
      Log("When child is shown the interstitial"),
      InstrumentTab(kChildElementId, tab_index, &child().browser()),
      NavigateWebContents(kChildElementId, mature_site_url),
      // The user clicks the local approval button.
      WaitForStateChange(kChildElementId, LocalApprovalButtonAppeared()),
      Log("When child requests local web approval"),
      ChildRequestsLocalApproval(kChildElementId),
      // The PACP dialog appears.
      Log("When the PACP dialog shows up"),
      WaitForShow(kLocalWebParentApprovalDialogId),
      InstrumentNonTabWebView(kPacpViewElementId,
                              kLocalWebParentApprovalDialogId),
      WaitForPacpDialogToAppear(kPacpViewElementId),
      Log("When parent approval dialog opens"),
      // Interact with a PACP web view element to ensure the webview is focused
      // and send ESC event to the webview..
      UserClicksPacpApprovalButton(kPacpViewElementId),
      SendAccelerator(kPacpViewElementId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(kPacpViewElementId), Log("When the user presses Escape"),
      Do([&]() {
        histogram_tester.ExpectBucketCount(
            "FamilyLinkUser.LocalWebApprovalResult",
            supervised_user::LocalApprovalResult::kCanceled, 1);
        histogram_tester.ExpectTotalCount(
            "FamilyLinkUser.LocalWebApprovalResult", 1);
      }),
      Log("Then the dialog is dismissed and a cancellation is recorded"))));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest,
                       ChildInPendingStateCanReauthAndRequestApproval) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildSignInElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);

  TurnOnSync();
  RunTestSequence(WaitForStateSeeding(kResetStateObserverId, child(),
                                      FamilyLinkSettingsState::Reset()));

  child().SignOutFromWeb();
  // TODO(b/364011203): Once the condition for displaying the interstitial is
  // set, expect the condition is met here.

  // Child activity starts in this tab.
  int tab_index = 0;
  RunTestSequence(
      // Child in pending state navigates to explicit website.
      Log("Test sequence starting"),
      InstrumentTab(kChildElementId, tab_index, &child().browser()),
      NavigateWebContents(kChildElementId,
                          GetRoutedUrl("https://bestgore.com")),
      Log("When child in pending state navigates to blocked url"),
      // Child is shown the re-authentication interstitial.
      WaitForStateChange(kChildElementId,
                         PageWithMatchingTitle("Site blocked")),
      WaitForStateChange(kChildElementId,
                         ReauthenticationInterstitialNextButtonAppeared()),
      Log("Then child is shown the re-authentication interstitial"),
      ChildProceedsToSignIn(kChildElementId),
      Log("When child clicks the 'Next' button on interstitial"),
      InstrumentTab(kChildSignInElementId, tab_index + 1, &child().browser()),
      WaitForStateChange(kChildSignInElementId,
                         PageWithMatchingTitle("Sign in - Google Accounts")),
      Log("The child is redirected to Sign-in page"),
      // Child goes through the sign-in flow.
      // Note: If the UI-sign performed below is flaky we can drop this part of
      // the test or find another way to sign-in.
      DoChildSignInFromUI(kChildSignInElementId),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),
      Log("The child is shown the blocked url interstitial"),
      EnsureNotPresent(kChildSignInElementId), Log("The sign-in tab is closed"),
      Log("Test sequence finished"));
}
#endif // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)


INSTANTIATE_TEST_SUITE_P(
    ,
    UrlFilterUiTest,
    testing::Values(FamilyLiveTest::RpcMode::kProd,
                    FamilyLiveTest::RpcMode::kTestImpersonation),
    [](const testing::TestParamInfo<FamilyLiveTest::RpcMode>& info) {
      return ToString(info.param);
    });

}  // namespace
}  // namespace supervised_user
