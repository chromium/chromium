// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/events/event_modifiers.h"

namespace {
const char kMainEmail[] = "main_email@example.com";
const InteractiveBrowserTest::DeepQuery kHistoryOptinAcceptButton = {
    "history-sync-optin-app", "#acceptButton"};
const InteractiveBrowserTest::DeepQuery kHistoryOptinRejectButton = {
    "history-sync-optin-app", "#rejectButton"};
const char kIsVisibleFn[] =
    "(el) => {"
    "  if (el.hidden) return false;"
    "  const style = window.getComputedStyle(el);"
    "  return style.display !== 'none' && style.visibility !== 'hidden';"
    "}";

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

// Sets up the account capability for history sync opt-in restriction mode.
// Without this capability, the fallback mode is used after a short delay.
void SetHistorySyncOptinRestrictionModeCapability(
    signin::IdentityTestEnvironment& environment,
    AccountInfo& account_info,
    bool is_unrestricted) {
  AccountCapabilitiesTestMutator(&account_info.capabilities)
      .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
          is_unrestricted);
  environment.UpdateAccountInfoForAccount(account_info);
}

// Tests that the history sync optin is displayed from promo entry points.
class HistorySyncOptinScreenFromPromoEntryPointInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>>,
      public testing::WithParamInterface<bool> {
 public:
  StateChange UiElementHasAppeared(DeepQuery element_selector) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = element_selector;
    state_change.event = kStateChange;
    state_change.test_function = kIsVisibleFn;
    return state_change;
  }

  auto ClickButton(ui::ElementIdentifier parent_element_id,
                   DeepQuery button_query) {
    return Steps(
        ExecuteJsAt(parent_element_id, button_query, "e => e.click()"));
  }

  bool IsUnrestricted() { return GetParam(); }

 protected:
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;

  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_P(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       ShowHistorySyncOptinScreenAfterSignin) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()),
      // Opens a sign-in tab as the method is called with an empty account.
      Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(), AccountInfo(),
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
      }),
      Do([&]() {
        content::WebContents* active_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        std::unique_ptr<ProcessDiceHeaderDelegateImpl>
            process_dice_header_delegate_impl =
                ProcessDiceHeaderDelegateImpl::Create(active_contents);
        AccountInfo account_info =
            identity_test_env()->MakeAccountAvailable(kMainEmail);
        SetHistorySyncOptinRestrictionModeCapability(
            *identity_test_env(), account_info, IsUnrestricted());
        // Mock processing an ENABLE SYNC header as part of the sign-in.
        // This also signs in the user.
        process_dice_header_delegate_impl->EnableSync(account_info);
      }),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      // Check that the buttons are visible.
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinAcceptButton, kIsVisibleFn, true),
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinRejectButton, kIsVisibleFn, true),
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       ShowHistorySyncOptinScreenForSignedInUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        SetHistorySyncOptinRestrictionModeCapability(
            *identity_test_env(), account_info, IsUnrestricted());
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      // Check that the buttons are visible.
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinAcceptButton, kIsVisibleFn, true),
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinRejectButton, kIsVisibleFn, true),
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      /*sample=*/
      IsUnrestricted()
          ? signin_metrics::SyncButtonsType::kSyncNotEqualWeighted
          : signin_metrics::SyncButtonsType::kSyncEqualWeightedFromCapability,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency",
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     /*expected_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(
    HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
    ShowHistorySyncOptinScreenForSignedInUserWithoutRestrictionCapability) {
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
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      // Check that the buttons are initially hidden.
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinAcceptButton, kIsVisibleFn, false),
      CheckJsResultAt(kHistorySyncOptinDialogContentsId,
                      kHistoryOptinRejectButton, kIsVisibleFn, false),
      // Wait until the buttons are visible.
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Completed",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.SyncButtons.Shown",
      /*sample=*/
      signin_metrics::SyncButtonsType::kSyncEqualWeightedFromDeadline,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectTotalCount(
      "Signin.AccountCapabilities.UserVisibleLatency",
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                     /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountCapabilities.ImmediatelyAvailable",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       DeclineHistorySyncOptin) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        SetHistorySyncOptinRestrictionModeCapability(
            *identity_test_env(), account_info, IsUnrestricted());
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Declined",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
                       HistorySyncOptinAbortedOnEscapeKey) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  AccountInfo account_info;

  RunTestSequence(
      Do([&]() {
        account_info = identity_test_env()->MakePrimaryAccountAvailable(
            kMainEmail, signin::ConsentLevel::kSignin);
        SetHistorySyncOptinRestrictionModeCapability(
            *identity_test_env(), account_info, IsUnrestricted());
      }),
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        signin_ui_util::EnableSyncFromSingleAccountPromo(
            browser()->profile(),
            /*account=*/account_info,
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
      }),
      // The user is already signed-in, the history sync optin dialog should
      // open.
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.HistorySyncOptIn.Aborted",
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
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
        SetHistorySyncOptinRestrictionModeCapability(
            *identity_test_env(), account_info, IsUnrestricted());
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
            signin_metrics::AccessPoint::kAccountMenuSwitchAccount);
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
      /*sample=*/signin_metrics::AccessPoint::kAccountMenuSwitchAccount,
      /*expected_count=*/1);
}

// This boolean parameter controls the value of the account capability
// `can_show_history_sync_opt_ins_without_minor_mode_restrictions`.
INSTANTIATE_TEST_SUITE_P(
    All,
    HistorySyncOptinScreenFromPromoEntryPointInteractiveTest,
    ::testing::Bool(),  // `true` for unrestricted, `false` for restricted.
    [](const testing::TestParamInfo<
        HistorySyncOptinScreenFromPromoEntryPointInteractiveTest::ParamType>&
           info) -> std::string {
      return info.param ? "Unrestricted" : "Restricted";
    });

class HistorySyncOptinScreenFromPromoEntryPointInteractiveTestWithTestSyncService
    : public HistorySyncOptinScreenFromPromoEntryPointInteractiveTest {
 protected:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    HistorySyncOptinScreenFromPromoEntryPointInteractiveTest::
        OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestSyncService));
  }
};
IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinScreenFromPromoEntryPointInteractiveTestWithTestSyncService,
    ShowsErrorDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  signin::AccountAvailabilityOptionsBuilder builder =
      identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.WithAccessPoint(signin_metrics::AccessPoint::kRecentTabs);

  // Mark the management as accepted, so that the disclaimer service progresses
  // the flow immediately on the present profile when
  // `EnsureManagedProfileForAccount` is invoked.
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);

  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(browser()->profile()));
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});
  sync_service->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kTabs, /*managed=*/true);

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()), Do([&]() {
        identity_test_env()->MakeAccountAvailable(
            builder.AsPrimary(signin::ConsentLevel::kSignin).Build(kMainEmail));
      }),
      WaitForShow(signin_util::kSigninErrorDialogId),
      WaitForShow(signin_util::kSigninErrorDialogOkButtonId),
      PressButton(signin_util::kSigninErrorDialogOkButtonId),
      WaitForHide(signin_util::kSigninErrorDialogOkButtonId));
}

}  // namespace
