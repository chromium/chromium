// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_interactive_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/password_generation_popup_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

class TestPopupObserver : public PasswordGenerationPopupObserver {
 public:
  enum class GenerationPopup {
    kShown,
    kHidden,
  };

  TestPopupObserver() = default;
  ~TestPopupObserver() = default;

  void OnPopupShown(
      PasswordGenerationPopupController::GenerationUIState state) override {
    popup_showing_ = GenerationPopup::kShown;
    state_ = state;
    MaybeQuitRunLoop();
  }

  void OnPopupHidden() override {
    popup_showing_ = GenerationPopup::kHidden;
    MaybeQuitRunLoop();
  }

  bool popup_showing() const {
    return popup_showing_ == GenerationPopup::kShown;
  }
  PasswordGenerationPopupController::GenerationUIState state() const {
    return state_;
  }

  // Waits until the popup is in specified status.
  void WaitForStatus(GenerationPopup status) {
    if (status == popup_showing_)
      return;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
    EXPECT_EQ(popup_showing_, status);
  }

  // Waits until the popup is either shown or hidden.
  void WaitForStatusChange() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop_->Run();
  }

 private:
  void MaybeQuitRunLoop() {
    if (run_loop_) {
      run_loop_->Quit();
      run_loop_ = nullptr;
    }
  }

  // The loop to be stopped after the popup state change.
  base::RunLoop* run_loop_ = nullptr;
  GenerationPopup popup_showing_ = GenerationPopup::kHidden;
  PasswordGenerationPopupController::GenerationUIState state_ =
      PasswordGenerationPopupController::kOfferGeneration;

  DISALLOW_COPY_AND_ASSIGN(TestPopupObserver);
};

enum ReturnCodes {  // Possible results of the JavaScript code.
  RETURN_CODE_OK,
  RETURN_CODE_NO_ELEMENT,
  RETURN_CODE_INVALID,
};

}  // namespace

class PasswordGenerationInteractiveTest
    : public PasswordManagerInteractiveTestBase {
 public:
  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    // Disable Autofill requesting access to AddressBook data. This will cause
    // the tests to hang on Mac.
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Set observer for popup.
    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());
    client->SetTestObserver(&observer_);
    // The base class should enable password generation.
    ASSERT_TRUE(client->GetPasswordFeatureManager()->IsGenerationEnabled());
    password_manager::PasswordFormManager::
        set_wait_for_server_predictions_for_filling(false);

    NavigateToFile("/password/signup_form_new_password.html");
  }

  void TearDownOnMainThread() override {
    PasswordManagerBrowserTestBase::TearDownOnMainThread();

    autofill::test::ReenableSystemServices();
  }

  // Waits until the value of the field with id |field_id| becomes non-empty.
  void WaitForNonEmptyFieldValue(const std::string& field_id) {
    const std::string script = base::StringPrintf(
        "element = document.getElementById('%s');"
        "if (!element) {"
        "  setTimeout(window.domAutomationController.send(%d), 0);"
        "}"
        "if (element.value) {"
        "  setTimeout(window.domAutomationController.send(%d), 0); "
        "} else {"
        "  element.onchange = function() {"
        "    if (element.value) {"
        "      window.domAutomationController.send(%d);"
        "    }"
        "  }"
        "}",
        field_id.c_str(), RETURN_CODE_NO_ELEMENT, RETURN_CODE_OK,
        RETURN_CODE_OK);
    int return_value = RETURN_CODE_INVALID;
    ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractInt(
        RenderFrameHost(), script, &return_value));
    EXPECT_EQ(RETURN_CODE_OK, return_value);
  }

  std::string GetFocusedElement() {
    std::string focused_element;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        WebContents(),
        "window.domAutomationController.send("
        "    document.activeElement.id)",
        &focused_element));
    return focused_element;
  }

  void FocusPasswordField() {
    ASSERT_TRUE(content::ExecuteScript(
        WebContents(), "document.getElementById('password_field').focus()"));
  }

  void FocusUsernameField() {
    ASSERT_TRUE(content::ExecuteScript(
        WebContents(), "document.getElementById('username_field').focus();"));
  }

  void SendKeyToPopup(ui::KeyboardCode key) {
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = key;
    WebContents()->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(
        event);
  }

  bool GenerationPopupShowing() {
    return observer_.popup_showing() &&
           observer_.state() ==
               PasswordGenerationPopupController::kOfferGeneration;
  }

  bool EditingPopupShowing() {
    return observer_.popup_showing() &&
           observer_.state() ==
               PasswordGenerationPopupController::kEditGeneratedPassword;
  }

  void WaitForStatus(TestPopupObserver::GenerationPopup status) {
    observer_.WaitForStatus(status);
  }

  void WaitForPopupStatusChange() { observer_.WaitForStatusChange(); }

 private:
  TestPopupObserver observer_;
};

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       PopupShownAndPasswordSelected) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());
  base::HistogramTester histogram_tester;
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Selecting the password should fill the field and move focus to the
  // submit button.
  WaitForNonEmptyFieldValue("password_field");
  EXPECT_FALSE(GenerationPopupShowing());
  EXPECT_FALSE(EditingPopupShowing());
  EXPECT_EQ("input_submit_button", GetFocusedElement());

  // Re-focusing the password field should show the editing popup.
  FocusPasswordField();
  EXPECT_TRUE(EditingPopupShowing());

  // The metrics are recorded when the form manager is destroyed. Closing the
  // tab enforces it.
  CloseAllBrowsers();
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.UserDecision",
      password_manager::PasswordFormMetricsRecorder::GeneratedPasswordStatus::
          kPasswordAccepted,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       PopupShownAutomaticallyAndPasswordErased) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Wait until the password is filled.
  WaitForNonEmptyFieldValue("password_field");

  // Re-focusing the password field should show the editing popup.
  FocusPasswordField();
  EXPECT_TRUE(EditingPopupShowing());

  // Delete the password. The generation prompt should be visible.
  base::HistogramTester histogram_tester;
  SimulateUserDeletingFieldContent("password_field");
  WaitForPopupStatusChange();
  EXPECT_FALSE(EditingPopupShowing());
  EXPECT_TRUE(GenerationPopupShowing());

  // The metrics are recorded on navigation when the frame is destroyed.
  NavigateToFile("/password/done.html");
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.UserDecision",
      password_manager::PasswordFormMetricsRecorder::GeneratedPasswordStatus::
          kPasswordDeleted,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       PopupShownManuallyAndPasswordErased) {
  NavigateToFile("/password/password_form.html");
  FocusPasswordField();
  EXPECT_FALSE(GenerationPopupShowing());
  // The same flow happens when user generates a password from the context menu.
  password_manager_util::UserTriggeredManualGenerationFromContextMenu(
      ChromePasswordManagerClient::FromWebContents(WebContents()));
  WaitForStatus(TestPopupObserver::GenerationPopup::kShown);
  EXPECT_TRUE(GenerationPopupShowing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Wait until the password is filled.
  WaitForNonEmptyFieldValue("password_field");

  // Re-focusing the password field should show the editing popup.
  FocusPasswordField();
  EXPECT_TRUE(EditingPopupShowing());

  // Delete the password. The generation prompt should not be visible.
  SimulateUserDeletingFieldContent("password_field");
  WaitForStatus(TestPopupObserver::GenerationPopup::kHidden);
  EXPECT_FALSE(EditingPopupShowing());
  EXPECT_FALSE(GenerationPopupShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_PopupShownAndDismissed) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());

  FocusUsernameField();

  // Popup is dismissed.
  WaitForStatus(TestPopupObserver::GenerationPopup::kHidden);
}
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       PopupShownAndDismissedByKeyPress) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());

  SendKeyToPopup(ui::VKEY_ESCAPE);

  // Popup is dismissed.
  EXPECT_FALSE(GenerationPopupShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       PopupShownAndDismissedByScrolling) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());

  ASSERT_TRUE(
      content::ExecuteScript(WebContents(), "window.scrollTo(100, 0);"));

  EXPECT_FALSE(GenerationPopupShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       GenerationTriggeredInIFrame) {
  NavigateToFile("/password/framed_signup_form.html");

  // Execute the script in the context of the iframe so that it kinda receives a
  // user gesture.
  std::vector<content::RenderFrameHost*> frames = WebContents()->GetAllFrames();
  ASSERT_EQ(2u, frames.size());
  ASSERT_TRUE(frames[0] == RenderFrameHost());

  std::string focus_script =
      "document.getElementById('password_field').focus();";

  ASSERT_TRUE(content::ExecuteScript(frames[1], focus_script));
  EXPECT_TRUE(GenerationPopupShowing());
}

// https://crbug.com/791389
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_AutoSavingGeneratedPassword) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());

  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Change username.
  FocusUsernameField();
  content::SimulateKeyPress(WebContents(), ui::DomKey::FromCharacter('U'),
                            ui::DomCode::US_U, ui::VKEY_U, false, false, false,
                            false);
  content::SimulateKeyPress(WebContents(), ui::DomKey::FromCharacter('N'),
                            ui::DomCode::US_N, ui::VKEY_N, false, false, false,
                            false);

  // Submit form.
  NavigationObserver observer(WebContents());
  std::string submit_script =
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(WebContents(), submit_script));
  observer.Wait();

  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());

  // Make sure the username is correct.
  password_manager::TestPasswordStore::PasswordMap stored_passwords =
      password_store->stored_passwords();
  EXPECT_EQ(1u, stored_passwords.size());
  EXPECT_EQ(1u, stored_passwords.begin()->second.size());
  EXPECT_EQ(base::UTF8ToUTF16("UN"),
            (stored_passwords.begin()->second)[0].username_value);
}
