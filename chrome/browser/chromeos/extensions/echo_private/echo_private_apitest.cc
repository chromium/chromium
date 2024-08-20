// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/notifications/echo_dialog_view.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"

namespace utils = extensions::api_test_utils;

namespace chromeos {

class ExtensionEchoPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  enum DialogTestAction {
    DIALOG_TEST_ACTION_NONE,
    DIALOG_TEST_ACTION_ACCEPT,
    DIALOG_TEST_ACTION_CANCEL,
  };

  ExtensionEchoPrivateApiTest()
      : expected_dialog_buttons_(
            static_cast<int>(ui::mojom::DialogButton::kNone)),
        dialog_action_(DIALOG_TEST_ACTION_NONE),
        dialog_invocation_count_(0) {}

  ~ExtensionEchoPrivateApiTest() override {}

  void SetUp() override {
    statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    statistics_provider_.SetMachineStatistic(ash::system::kOffersCouponCodeKey,
                                             "COUPON_CODE");
    statistics_provider_.SetMachineStatistic(ash::system::kOffersGroupCodeKey,
                                             "GROUP_CODE");
    statistics_provider_.SetMachineStatistic(ash::system::kActivateDateKey,
                                             "2024-13");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    ash::EchoDialogView::AddShowCallbackForTesting(base::BindOnce(
        &ExtensionEchoPrivateApiTest::OnDialogShown, base::Unretained(this)));
    extensions::ExtensionApiTest::SetUp();
  }

  void RunDefaultGetUserFunctionAndExpectResultEquals(int tab_id,
                                                      bool expected_result) {
    auto function = base::MakeRefCounted<EchoPrivateGetUserConsentFunction>();
    function->set_has_callback(true);

    const std::string arguments = base::StringPrintf(
        R"([{"serviceName": "name", "origin": "https://test.com", "tabId": %d}])",
        tab_id);
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), arguments, profile());

    ASSERT_TRUE(result);
    ASSERT_EQ(base::Value::Type::BOOLEAN, result->type());

    EXPECT_EQ(expected_result, result->GetBool());
  }

  void OnDialogShown(ash::EchoDialogView* dialog) {
    dialog_invocation_count_++;
    ASSERT_LE(dialog_invocation_count_, 1);

    EXPECT_EQ(expected_dialog_buttons_, dialog->buttons());

    // Don't accept the dialog if the dialog buttons don't match expectation.
    // Accepting a dialog which should not have accept option may crash the
    // test. The test already failed, so it's ok to cancel the dialog.
    DialogTestAction dialog_action = dialog_action_;
    if (dialog_action == DIALOG_TEST_ACTION_ACCEPT &&
        expected_dialog_buttons_ != dialog->buttons()) {
      dialog_action = DIALOG_TEST_ACTION_CANCEL;
    }

    // Perform test action on the dialog.
    // The dialog should stay around until AcceptWindow or CancelWindow is
    // called, so base::Unretained is safe.
    if (dialog_action == DIALOG_TEST_ACTION_ACCEPT) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&ash::EchoDialogView::Accept),
                         base::Unretained(dialog)));
    } else if (dialog_action == DIALOG_TEST_ACTION_CANCEL) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&ash::EchoDialogView::Cancel),
                         base::Unretained(dialog)));
    }
  }

  int dialog_invocation_count() const {
    return dialog_invocation_count_;
  }

  // Open and activates tab in the test browser. Returns the ID of the opened
  // tab.
  int OpenAndActivateTab() {
    EXPECT_TRUE(
        AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
    browser()->tab_strip_model()->ActivateTabAt(
        0, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    return extensions::ExtensionTabUtil::GetTabId(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  bool CloseTabWithId(int tab_id) {
    TabStripModel* tab_strip = nullptr;
    int tab_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(tab_id, profile(), false,
                                                  nullptr, &tab_strip, nullptr,
                                                  &tab_index)) {
      ADD_FAILURE() << "Tab not found " << tab_id;
      return false;
    }

    int previous_tab_count = tab_strip->count();
    tab_strip->CloseWebContentsAt(tab_index, 0);
    return (previous_tab_count - 1) == tab_strip->count();
  }

 protected:
  int expected_dialog_buttons_;
  DialogTestAction dialog_action_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::system::FakeStatisticsProvider statistics_provider_;

 private:
  int dialog_invocation_count_;
};

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, EchoTest) {
  EXPECT_TRUE(RunExtensionTest("echo/component_extension", {},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_InvalidOrigin) {
  const int tab_id = OpenAndActivateTab();

  expected_dialog_buttons_ = static_cast<int>(ui::mojom::DialogButton::kNone);
  dialog_action_ = DIALOG_TEST_ACTION_NONE;

  auto function = base::MakeRefCounted<EchoPrivateGetUserConsentFunction>();

  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      base::StringPrintf(
          R"([{"serviceName": "name", "origin": "invalid", "tabId": %d}])",
          tab_id),
      profile());

  EXPECT_EQ("Invalid origin.", error);
  EXPECT_EQ(0, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, GetUserConsent_NoTabIdSet) {
  expected_dialog_buttons_ = static_cast<int>(ui::mojom::DialogButton::kNone);
  dialog_action_ = DIALOG_TEST_ACTION_NONE;

  auto function = base::MakeRefCounted<EchoPrivateGetUserConsentFunction>();

  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      R"([{"serviceName": "name", "origin": "https://test.com"}])", profile());

  EXPECT_EQ("Not called from an app window - the tabId is required.", error);
  EXPECT_EQ(0, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_InactiveTab) {
  const int tab_id = OpenAndActivateTab();
  // Open and activate another tab.
  OpenAndActivateTab();

  expected_dialog_buttons_ = static_cast<int>(ui::mojom::DialogButton::kNone);
  dialog_action_ = DIALOG_TEST_ACTION_NONE;

  auto function = base::MakeRefCounted<EchoPrivateGetUserConsentFunction>();

  const std::string arguments = base::StringPrintf(
      R"([{"serviceName": "name", "origin": "https://test.com", "tabId": %d}])",
      tab_id);
  std::string error =
      utils::RunFunctionAndReturnError(function.get(), arguments, profile());

  EXPECT_EQ("Consent requested from an inactive tab.", error);
  EXPECT_EQ(0, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, GetUserConsent_ClosedTab) {
  const int tab_id = OpenAndActivateTab();
  ASSERT_TRUE(CloseTabWithId(tab_id));

  expected_dialog_buttons_ = static_cast<int>(ui::mojom::DialogButton::kNone);
  dialog_action_ = DIALOG_TEST_ACTION_NONE;

  auto function = base::MakeRefCounted<EchoPrivateGetUserConsentFunction>();

  const std::string arguments = base::StringPrintf(
      R"([{"serviceName": "name", "origin": "https://test.com", "tabId": %d}])",
      tab_id);
  std::string error =
      utils::RunFunctionAndReturnError(function.get(), arguments, profile());

  EXPECT_EQ("Tab not found.", error);
  EXPECT_EQ(0, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefNotSet) {
  const int tab_id = OpenAndActivateTab();
  expected_dialog_buttons_ =
      static_cast<int>(ui::mojom::DialogButton::kCancel) |
      static_cast<int>(ui::mojom::DialogButton::kOk);
  dialog_action_ = DIALOG_TEST_ACTION_ACCEPT;

  RunDefaultGetUserFunctionAndExpectResultEquals(tab_id, true);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefTrue) {
  const int tab_id = OpenAndActivateTab();
  scoped_testing_cros_settings_.device_settings()->Set(
      ash::kAllowRedeemChromeOsRegistrationOffers, base::Value(true));

  expected_dialog_buttons_ =
      static_cast<int>(ui::mojom::DialogButton::kCancel) |
      static_cast<int>(ui::mojom::DialogButton::kOk);
  dialog_action_ = DIALOG_TEST_ACTION_ACCEPT;

  RunDefaultGetUserFunctionAndExpectResultEquals(tab_id, true);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_ConsentDenied) {
  const int tab_id = OpenAndActivateTab();
  scoped_testing_cros_settings_.device_settings()->Set(
      ash::kAllowRedeemChromeOsRegistrationOffers, base::Value(true));

  expected_dialog_buttons_ =
      static_cast<int>(ui::mojom::DialogButton::kCancel) |
      static_cast<int>(ui::mojom::DialogButton::kOk);
  dialog_action_ = DIALOG_TEST_ACTION_CANCEL;

  RunDefaultGetUserFunctionAndExpectResultEquals(tab_id, false);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefFalse) {
  const int tab_id = OpenAndActivateTab();
  scoped_testing_cros_settings_.device_settings()->Set(
      ash::kAllowRedeemChromeOsRegistrationOffers, base::Value(false));

  expected_dialog_buttons_ = static_cast<int>(ui::mojom::DialogButton::kCancel);
  dialog_action_ = DIALOG_TEST_ACTION_CANCEL;

  RunDefaultGetUserFunctionAndExpectResultEquals(tab_id, false);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, RemoveEmptyValueDicts) {
  auto dict = base::Value::Dict()
                  .Set("a", "b")
                  .Set("empty", base::Value::Dict())
                  .Set("nested", base::Value::Dict().Set("c", "d").Set(
                                     "empty_value", base::Value::Dict()))
                  .Set("nested_empty", base::Value::Dict().Set(
                                           "empty_value", base::Value::Dict()));

  // Remove nested dictionaries.
  chromeos::echo_offer::RemoveEmptyValueDicts(dict);

  // After removing empty nested dicts, we  are left with:
  //   {"a" : "b", "nested" : {"c" : "d"}}
  EXPECT_EQ(2u, dict.size());
  EXPECT_EQ(1u, dict.FindDict("nested")->size());
  EXPECT_EQ("b", *dict.FindString("a"));
  EXPECT_EQ("d", *dict.FindStringByDottedPath("nested.c"));
}

}  // namespace chromeos
