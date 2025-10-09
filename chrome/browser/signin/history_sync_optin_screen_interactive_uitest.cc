// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/events/event_modifiers.h"

namespace {
const char kMainEmail[] = "main_email@example.com";
const InteractiveBrowserTest::DeepQuery kHistoryOptinAcceptButton = {
    "history-sync-optin-app", "#acceptButton"};
const InteractiveBrowserTest::DeepQuery kHistoryOptinRejectButton = {
    "history-sync-optin-app", "#rejectButton"};

// Simulates the account capabilities that make the user eligible for the
// history sync opt-in, so that the UI is preconfigured to show the opt-in
// without any delay and wait-ui. Otherwise, UI should be presenting some sort
// of loading UI and clicking reject or accept buttons should not be available.
void MakeHistorySyncOptinEligible(signin::IdentityTestEnvironment& environment,
                                  AccountInfo& account_info) {
  AccountCapabilitiesTestMutator(&account_info.capabilities)
      .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(true);
  environment.UpdateAccountInfoForAccount(account_info);
}

// Tests that the history sync optin is displayed from promo entry points.
class HistorySyncOptinScreenFromPromoEntryPointInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  StateChange UiElementHasAppeared(DeepQuery element_selector) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kExists;
    state_change.where = element_selector;
    state_change.event = kStateChange;
    return state_change;
  }

  auto ClickButton(ui::ElementIdentifier parent_element_id,
                   DeepQuery button_query) {
    return Steps(
        ExecuteJsAt(parent_element_id, button_query, "e => e.click()"));
  }

 protected:
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;

  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_F(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       ShowHistorySyncOptinScreenAfterSignin) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()),
      // Opens a sign-in tab as the method is called with an empty account.
      Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(), AccountInfo(),
            signin_metrics::AccessPoint::kAccountMenu);
      }),
      Do([&]() {
        content::WebContents* active_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        std::unique_ptr<ProcessDiceHeaderDelegateImpl>
            process_dice_header_delegate_impl =
                ProcessDiceHeaderDelegateImpl::Create(active_contents);
        AccountInfo account_info =
            identity_test_env()->MakeAccountAvailable(kMainEmail);
        MakeHistorySyncOptinEligible(*identity_test_env(), account_info);
        // Mock processing an ENABLE SYNC header as part of the sign-in.
        // This also signs in the user.
        process_dice_header_delegate_impl->EnableSync(account_info);
      }),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)),
      ClickButton(kHistorySyncOptinDialogContentsId, kHistoryOptinAcceptButton),
      WaitForHide(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_TRUE(SyncServiceFactory::GetForProfile(browser()->profile())
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kHistory));

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       ShowHistorySyncOptinScreenForSignedInUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        MakeHistorySyncOptinEligible(*identity_test_env(), account_info);
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenu);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)),
      ClickButton(kHistorySyncOptinDialogContentsId, kHistoryOptinAcceptButton),
      WaitForHide(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_TRUE(SyncServiceFactory::GetForProfile(browser()->profile())
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kHistory));

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       DeclineHistorySyncOptin) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        MakeHistorySyncOptinEligible(*identity_test_env(), account_info);
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenu);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)),
      // Use kFireAndForget because clicking the reject button closes the
      // dialog immediately, causing the default visibility check to fail.
      ExecuteJsAt(kHistorySyncOptinDialogContentsId, kHistoryOptinRejectButton,
                  "e => e.click()",
                  InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget),
      WaitForHide(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_FALSE(SyncServiceFactory::GetForProfile(browser()->profile())
                   ->GetUserSettings()
                   ->GetSelectedTypes()
                   .Has(syncer::UserSelectableType::kHistory));

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Declined",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       HistorySyncOptinAbortedOnEscapeKey) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenu);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)),
      // Press the Escape key, dismissing the UI.
      SendAccelerator(kHistorySyncOptinDialogContentsId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_FALSE(SyncServiceFactory::GetForProfile(browser()->profile())
                   ->GetUserSettings()
                   ->GetSelectedTypes()
                   .Has(syncer::UserSelectableType::kHistory));

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Signin_HistorySync_AlreadyOptedIn"),
      0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Started",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Aborted",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       HistorySyncOptinSkippedIfUserIsAlreadyOptedIn) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingStateObserver<int>,
      kHistorySyncOptInAlreadyOptedInHistogramState);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        // Optin to syncing history, tabs & tab groups.
        auto* user_settings =
            SyncServiceFactory::GetForProfile(browser()->profile())
                ->GetUserSettings();
        user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                       true);
        user_settings->SetSelectedType(syncer::UserSelectableType::kTabs, true);
        user_settings->SetSelectedType(
            syncer::UserSelectableType::kSavedTabGroups, true);
      }),
      InstrumentTab(kTabId, 0, browser()),
      // Poll for the histogram to be recorded.
      PollState(kHistorySyncOptInAlreadyOptedInHistogramState,
                [&]() {
                  return user_action_tester_.GetActionCount(
                      "Signin_HistorySync_AlreadyOptedIn");
                }),
      Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenu);
      }),
      WaitForState(kHistorySyncOptInAlreadyOptedInHistogramState, 1),
      StopObservingState(kHistorySyncOptInAlreadyOptedInHistogramState),
      // The user is already opted in history/tab/tab grous syncing,
      // the history sync optin dialog should not open.
      EnsureNotPresent(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Started"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Completed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Declined"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Aborted"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Signin_HistorySync_Skipped"),
            0);
  histogram_tester_.ExpectTotalCount("Signin.HistorySyncOptIn.Started",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.AlreadyOptedIn",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenu,
      /*expected_count=*/1);
}

}  // namespace
