// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/state_observer.h"

namespace {
const char kMainEmail[] = "main_email@example.com";
const InteractiveBrowserTest::DeepQuery kHistoryOptinAcceptButton = {
    "history-sync-optin-app", "#acceptButton"};
const InteractiveBrowserTest::DeepQuery kHistoryOptinRejectButton = {
    "history-sync-optin-app", "#rejectButton"};
}  // namespace

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

 private:
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
        CoreAccountInfo account_info =
            identity_test_env()->MakeAccountAvailable(kMainEmail);
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
}
