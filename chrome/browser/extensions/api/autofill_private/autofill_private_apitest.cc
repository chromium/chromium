// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/autofill_private.h"

#include <memory>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/command_line.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
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
    autofill_client()->GetPersonalDataManager()->SetPrefService(
        autofill_client()->GetPrefs());
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
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
};

}  // namespace

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
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateAddress")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddAndUpdateCreditCard) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard")) << message_;
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

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, isValidIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("isValidIban")) << message_;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest,
                       authenticateUserAndFlipMandatoryAuthToggle) {
  base::UserActionTester user_action_tester;
  auto* mock_mandatory_reauth_manager =
      autofill_client()->GetOrCreatePaymentsMandatoryReauthManager();

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
                       authenticateUserToEditLocalCard) {
  base::UserActionTester user_action_tester;

  autofill_client()
      ->GetPersonalDataManager()
      ->SetPaymentMethodsMandatoryReauthEnabled(true);
  auto* mock_mandatory_reauth_manager =
      autofill_client()->GetOrCreatePaymentsMandatoryReauthManager();

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
  EXPECT_TRUE(RunAutofillSubtest("authenticateUserToEditLocalCard"))
      << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "PaymentsUserAuthSuccessfulToShowEditLocalCardDialog"));
}
#endif

}  // namespace extensions
