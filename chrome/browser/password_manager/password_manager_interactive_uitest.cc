// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_manager_interactive_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/new_password_form_manager.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test_utils.h"

namespace password_manager {

// Test fixture that condionally enable feature kAutofillExpandedPopupViews.
// The fixture should be replaced with PasswordManagerBrowserTestBase once the
// feature is deleted.
//
// Test params:
//  - bool popup_views_enabled: whether feature AutofillExpandedPopupViews
//        is enabled for testing.
class PasswordManagerInteractiveTest
    : public PasswordManagerInteractiveTestBase {
 public:
  PasswordManagerInteractiveTest() {
    // Turn off waiting for server predictions before filing. It makes filling
    // behaviour more deterministic. Filling with server predictions is tested
    // in NewPasswordFormManager unit tests.
    password_manager::NewPasswordFormManager::
        set_wait_for_server_predictions_for_filling(false);
  }
  ~PasswordManagerInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest, UsernameChanged) {
  // At first let us save a credential to the password store.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("random");
  password_store->AddLogin(signin_form);

  // Load the page to have the saved credentials autofilled.
  NavigateToFile("/password/signup_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("username_field", "temp");
  WaitForElementValue("password_field", "random");

  // Change username and submit. This should add the characters "orary" to the
  // already autofilled username.
  FillElementWithValue("username_field", "orary");

  // Move the focus out of the inputs before waiting because WaitForElementValue
  // uses "onchange" event. The event is triggered only when the control looses
  // focus.
  chrome::FocusLocationBar(browser());
  WaitForElementValue("username_field", "temporary");

  NavigationObserver navigation_observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string submit =
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(WebContents(), submit));
  navigation_observer.Wait();
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());

  // Verify that there are two saved password, the old password and the new
  // password.
  password_manager::TestPasswordStore::PasswordMap stored_passwords =
      password_store->stored_passwords();
  EXPECT_EQ(1u, stored_passwords.size());
  EXPECT_EQ(2u, stored_passwords.begin()->second.size());
  EXPECT_EQ(base::UTF8ToUTF16("temp"),
            (stored_passwords.begin()->second)[0].username_value);
  EXPECT_EQ(base::UTF8ToUTF16("temporary"),
            (stored_passwords.begin()->second)[1].username_value);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving) {
  NavigateToFile("/password/password_form.html");

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // The save prompt should be available but shouldn't pop up automatically.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Simulate several navigations.
  NavigateToFile("/password/signup_form.html");
  NavigateToFile("/password/failed.html");
  NavigateToFile("/password/done.html");

  // The save prompt should be still available.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("", "123");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_HideAfterTimeout) {
  NavigateToFile("/password/password_form.html");
  ManagePasswordsUIController::set_save_fallback_timeout_in_seconds(0);

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Since the timeout is changed to zero for testing, the save prompt should be
  // hidden right after show.
  prompt_observer.WaitForInactiveState();
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_HideIcon) {
  NavigateToFile("/password/password_form.html");

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Delete typed content and verify that inactive state is reached.
  SimulateUserDeletingFieldContent("password_field");
  prompt_observer.WaitForInactiveState();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       ManualFallbackForSaving_GoToManagedState) {
  // At first let us save a credential to the password store.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("random");
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  FillElementWithValue("password_field", "123");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Delete typed content and verify that management state is reached.
  SimulateUserDeletingFieldContent("password_field");
  prompt_observer.WaitForManagementState();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForXHRWithoutOnSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if XHR navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  FillElementWithValue("username_field", "user");
  FillElementWithValue("password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_xhr()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForXHRWithNewPasswordsWithoutOnSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that if XHR navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  // Specifically verify that the password form saving new passwords is treated
  // the same as a login form.
  FillElementWithValue("signup_username_field", "user");
  FillElementWithValue("signup_password_field", "1234");
  FillElementWithValue("confirmation_password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_xhr()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

// Disabled for flakiness crbug.com/849582.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_PromptForFetchWithoutOnSubmit) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Verify that if Fetch navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  FillElementWithValue("username_field", "user");
  FillElementWithValue("password_field", "1234");

  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_fetch()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       PromptForFetchWithNewPasswordsWithoutOnSubmit) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Verify that if Fetch navigation occurs and the form is properly filled out,
  // we try and save the password even though onsubmit hasn't been called.
  // Specifically verify that the password form saving new passwords is treated
  // the same as a login form.
  FillElementWithValue("signup_username_field", "user");
  FillElementWithValue("signup_password_field", "1234");
  FillElementWithValue("confirmation_password_field", "1234");
  NavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(WebContents(), "send_fetch()"));
  observer.Wait();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

// Disabled for flakiness crbug.com/849582.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_AutofillPasswordFormWithoutUsernameField) {
  std::string submit = "document.getElementById('submit-button').click();";
  VerifyPasswordIsSavedAndFilled("/password/form_with_only_password_field.html",
                                 std::string(), "password", submit);
}

// Disabled for flakiness crbug.com/849582.
// Tests that if a site embeds the login and signup forms into one <form>, the
// login form still gets autofilled.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_AutofillLoginSignupForm) {
  std::string submit = "document.getElementById('submit').click();";
  VerifyPasswordIsSavedAndFilled("/password/login_signup_form.html", "username",
                                 "password", submit);
}

// Disabled for flakiness crbug.com/849582.
// Tests that password suggestions still work if the fields have the
// "autocomplete" attribute set to off.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_AutofillPasswordFormWithAutocompleteOff) {
  std::string submit = "document.getElementById('submit').click();";
  VerifyPasswordIsSavedAndFilled(
      "/password/password_autocomplete_off_test.html", "username", "password",
      submit);
}

// Disabled for flakiness crbug.com/849582.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_AutofillPasswordNoFormElement) {
  VerifyPasswordIsSavedAndFilled("/password/no_form_element.html",
                                 "username_field", "password_field",
                                 "send_xhr();");
}

// Disabled for flakiness crbug.com/849582.
// Check that we can fill in cases where <base href> is set and the action of
// the form is not set. Regression test for https://crbug.com/360230.
IN_PROC_BROWSER_TEST_F(PasswordManagerInteractiveTest,
                       DISABLED_AutofillBaseTagWithNoActionTest) {
  std::string submit = "document.getElementById('submit_button').click();";
  VerifyPasswordIsSavedAndFilled("/password/password_xhr_submit.html",
                                 "username_field", "password_field", submit);
}

}  // namespace password_manager
