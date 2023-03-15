// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

using ContextType = ExtensionBrowserTest::ContextType;

class AutofillPrivateApiTest : public ExtensionApiTest,
                               public testing::WithParamInterface<ContextType> {
 public:
  AutofillPrivateApiTest() : ExtensionApiTest(GetParam()) {}
  AutofillPrivateApiTest(const AutofillPrivateApiTest&) = delete;
  AutofillPrivateApiTest& operator=(const AutofillPrivateApiTest&) = delete;
  ~AutofillPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunAutofillSubtest(const char* subtest) {
    autofill::WaitForPersonalDataManagerToBeLoaded(profile());

    return RunExtensionTest("autofill_private", {.custom_arg = subtest},
                            {.load_as_component = true});
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutofillPrivateApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutofillPrivateApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace

// TODO(hcarmona): Investigate converting these tests to unittests.

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, GetCountryList) {
  EXPECT_TRUE(RunAutofillSubtest("getCountryList")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, GetAddressComponents) {
  EXPECT_TRUE(RunAutofillSubtest("getAddressComponents")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, RemoveEntry) {
  EXPECT_TRUE(RunAutofillSubtest("removeEntry")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, ValidatePhoneNumbers) {
  EXPECT_TRUE(RunAutofillSubtest("validatePhoneNumbers")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, AddAndUpdateAddress) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateAddress")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, AddAndUpdateCreditCard) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, AddNewIban_NoNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addNewIbanNoNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanAdded"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("AutofillIbanAddedWithNickname"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, AddNewIban_WithNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("addNewIbanWithNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanAdded"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("AutofillIbanAddedWithNickname"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, noChangesToExistingIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("noChangesToExistingIban")) << message_;
  EXPECT_EQ(0, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, updateExistingIbanNoNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("updateExistingIbanNoNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, updateExistingIbanWithNickname) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("updateExistingIbanWithNickname")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanEdited"));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("AutofillIbanEditedWithNickname"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, removeExistingIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("removeExistingIban")) << message_;
  EXPECT_EQ(1, user_action_tester.GetActionCount("AutofillIbanDeleted"));
}

IN_PROC_BROWSER_TEST_P(AutofillPrivateApiTest, isValidIban) {
  base::UserActionTester user_action_tester;
  EXPECT_TRUE(RunAutofillSubtest("isValidIban")) << message_;
}

}  // namespace extensions
