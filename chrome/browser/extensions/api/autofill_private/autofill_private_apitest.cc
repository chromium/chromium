// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {
namespace {

class AutofillPrivateApiTest : public ExtensionApiTest {
 public:
  AutofillPrivateApiTest() = default;
  AutofillPrivateApiTest(const AutofillPrivateApiTest&) = delete;
  AutofillPrivateApiTest& operator=(const AutofillPrivateApiTest&) = delete;
  ~AutofillPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
    // Rebinding the `autofill_client()` test PDM on the
    // `AutofillPrivateEventRouter`. This sets the correct test PDM instance on
    // the observers under the `AutofillPrivateEventRouter`.
    AutofillPrivateEventRouterFactory::GetForProfile(browser_context())
        ->RebindPersonalDataManagerForTesting(
            autofill_client()->GetPersonalDataManager());
    autofill_client()->GetPersonalDataManager()->SetPrefService(
        autofill_client()->GetPrefs());
  }

  void TearDownOnMainThread() override {
    // Unbinding the `autofill_client()` test PDM on the
    // `AutofillPrivateEventRouter`. This removes the test PDM instance added to
    // the observers in `SetUpOnMainThread()` for `AutofillPrivateEventRouter`.
    AutofillPrivateEventRouterFactory::GetForProfile(browser_context())
        ->UnbindPersonalDataManagerForTesting();
  }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    autofill::WaitForPersonalDataManagerToBeLoaded(profile());

    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("autofill_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_
        [browser()->tab_strip_model()->GetActiveWebContents()];
  }

 private:
  content::BrowserContext* browser_context() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetBrowserContext();
  }

  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
};

// TODO(hcarmona): Investigate converting these tests to unittests.

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, GetCountryList) {
  EXPECT_TRUE(RunAutofillSubtest("getCountryList")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, GetAddressComponents) {
  EXPECT_TRUE(RunAutofillSubtest("getAddressComponents")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, RemoveEntry) {
  EXPECT_TRUE(RunAutofillSubtest("removeEntry")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddAndUpdateAddress) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateAddress")) << message_;
  EXPECT_EQ(histogram_tester.GetAllSamples("Autofill.AddedNewAddress").size(),
            1u)
      << "Two tests are being run: addNewAddress and updateExistingAddress. "
         "'Autofill.AddedNewAddress' should be emitted  once for the first "
         "test.";
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddAndUpdateCreditCard) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddCreditCard_Cvc) {
  base::UserActionTester user_action_tester;

  EXPECT_TRUE(RunAutofillSubtest("addNewCreditCard")) << message_;

  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillCreditCardsAdded"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("AutofillCreditCardsAddedWithCvc"));
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiTest,
    AddCreditCard_Metrics_StoredCreditCardCountBeforeCardAdded) {
  base::HistogramTester histogram_tester;

  autofill::TestPersonalDataManager* personal_data_manager =
      autofill_client()->GetPersonalDataManager();
  // Required for adding the server card.
  personal_data_manager->payments_data_manager().SetSyncingForTest(
      /*is_syncing_for_test=*/true);

  // Set up the personal data manager with 2 existing cards.
  personal_data_manager->payments_data_manager().AddCreditCard(
      autofill::test::GetCreditCard2());
  personal_data_manager->test_payments_data_manager().AddServerCreditCard(
      autofill::test::GetMaskedServerCard());
  EXPECT_EQ(
      personal_data_manager->payments_data_manager().GetCreditCards().size(),
      2u);

  EXPECT_TRUE(RunAutofillSubtest("addNewCreditCard")) << message_;

  // Expect the metric to add a record for the 2 existing cards.
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.SettingsPage."
      "StoredCreditCardCountBeforeCardAdded",
      2, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddCreditCard_NoCvc) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addNewCreditCardWithoutCvc")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillCreditCardsAdded"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("AutofillCreditCardsAddedWithCvc"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       UpdateCreditCard_NoExistingCvc_NoNewCvcAdded) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasLeftBlank"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       UpdateCreditCard_NoExistingCvc_NewCvcAdded) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard_AddCvc")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasAdded"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       UpdateCreditCard_ExistingCvc_Removed) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard_RemoveCvc"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasRemoved"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       UpdateCreditCard_ExistingCvc_Updated) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard_UpdateCvc"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUpdated"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       UpdateCreditCard_ExistingCvc_Unchanged) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard_UnchangedCvc"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardsEditedAndCvcWasUnchanged"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddNewIban_NoNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addNewIbanNoNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanAdded"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("AutofillIbanAddedWithNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddNewIban_WithNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addNewIbanWithNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanAdded"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("AutofillIbanAddedWithNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, noChangesToExistingIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("noChangesToExistingIban")) << message_;
  EXPECT_EQ(0, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, updateExistingIbanNoNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("updateExistingIbanNoNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, updateExistingIbanWithNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("updateExistingIbanWithNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, removeExistingIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("removeExistingIban")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanDeleted"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, removeExistingCard) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("removeExistingCard")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillCreditCardDeleted"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       removeExistingCard_WithCvcAndNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("removeExistingCard_WithCvcAndNickname"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadCvc"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadNickname"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, isValidIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("isValidIban")) << message_;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       authenticateUserAndFlipMandatoryAuthToggle) {
  base::UserActionTester user_action_tester;
  auto* mock_mandatory_reauth_manager =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetOrCreatePaymentsMandatoryReauthManager();

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              mock_mandatory_reauth_manager),
          AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  EXPECT_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
                  mock_mandatory_reauth_manager),
              AuthenticateWithMessage)
      .Times(1);
  EXPECT_TRUE(RunAutofillSubtest("authenticateUserAndFlipMandatoryAuthToggle"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthTriggeredForMandatoryAuthToggle"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthSuccessfulForMandatoryAuthToggle"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       showEditCardDialogForLocalCard_ReauthOn) {
  base::UserActionTester user_action_tester;

  autofill_client()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(true);
  auto* mock_mandatory_reauth_manager =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetOrCreatePaymentsMandatoryReauthManager();

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              mock_mandatory_reauth_manager),
          AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  EXPECT_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
                  mock_mandatory_reauth_manager),
              AuthenticateWithMessage)
      .Times(1);
  EXPECT_TRUE(RunAutofillSubtest("getLocalCard")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthSuccessfulToShowEditLocalCardDialog"));
}
#endif

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       showEditCardDialogForLocalCard_ReauthOff) {
  base::UserActionTester user_action_tester;

  autofill_client()
      ->GetPersonalDataManager()
      ->payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(false);
  auto* mock_mandatory_reauth_manager =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetOrCreatePaymentsMandatoryReauthManager();

  EXPECT_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
                  mock_mandatory_reauth_manager),
              AuthenticateWithMessage)
      .Times(0);
  EXPECT_TRUE(RunAutofillSubtest("getLocalCard")) << message_;
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "PaymentsUserAuthSuccessfulToShowEditLocalCardDialog"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, bulkDeleteAllCvcs) {
  EXPECT_TRUE(RunAutofillSubtest("bulkDeleteAllCvcs")) << message_;
}

}  // namespace
}  // namespace extensions
