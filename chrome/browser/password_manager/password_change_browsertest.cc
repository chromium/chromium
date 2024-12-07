// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/test/run_until.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using affiliations::AffiliationService;
using affiliations::MockAffiliationService;

namespace {

class MockPasswordChangeDelegateObserver
    : public PasswordChangeDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (PasswordChangeDelegate::State),
              (override));
  MOCK_METHOD(void,
              OnPasswordChangeStopped,
              (PasswordChangeDelegate*),
              (override));
};

std::unique_ptr<KeyedService> CreateTestAffiliationService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockAffiliationService>>();
}

content::WebContents* OpenNewTabInBackground(base::WeakPtr<Browser> browser,
                                             const GURL& url,
                                             content::WebContents*) {
  if (!browser) {
    return nullptr;
  }
  int preexisting_tab = browser->tab_strip_model()->active_index();
  content::WebContents* contents = PasswordManagerBrowserTestBase::GetNewTab(
      browser.get(), /*open_new_tab=*/true);
  ui_test_utils::NavigateToURLWithDisposition(
      browser.get(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  browser->tab_strip_model()->ActivateTabAt(preexisting_tab);
  return contents;
}

}  // namespace

class PasswordChangeBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(&CreateTestAffiliationService));
                }));
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
  }

  MockAffiliationService* affiliation_service() {
    return static_cast<MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(browser()->profile()));
  }

  ChromePasswordChangeService* password_change_service() {
    return PasswordChangeServiceFactory::GetForProfile(browser()->profile());
    ;
  }

  // This allows to attach a custom ManagePasswordsUIController to intercept UI
  // interactions.
  void InterceptNewTabsOpening() {
    // TODO(crbug.com/382652112): find a way to observe bubbles without
    // intercepting new tab creation.
    password_change_service()->SetCustomTabOpening(
        base::BindRepeating(&OpenNewTabInBackground, browser()->AsWeakPtr()));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<PasswordChangeBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       StartingPasswordChangeOpensNewTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Assert that there is a single tab.
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_FALSE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));

  GURL main_url("https://example.com/"),
      change_pwd_url("https://example.com/password/");
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(change_pwd_url));

  password_change_service()->StartPasswordChange(main_url, u"test", u"password",
                                                 WebContents());

  // Verify a new tab is added, although the focus remained on the initial tab.
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());

  // Verify a new tab is opened with a change pwd url.
  EXPECT_EQ(change_pwd_url, tab_strip->GetWebContentsAt(1)->GetURL());

  // Verify that GetPasswordChangeDelegate() returns delegate for both tabs.
  EXPECT_TRUE(password_change_service()->GetPasswordChangeDelegate(
      tab_strip->GetWebContentsAt(0)));
  EXPECT_TRUE(password_change_service()->GetPasswordChangeDelegate(
      tab_strip->GetWebContentsAt(1)));
  EXPECT_EQ(password_change_service()->GetPasswordChangeDelegate(
                tab_strip->GetWebContentsAt(0)),
            password_change_service()->GetPasswordChangeDelegate(
                tab_strip->GetWebContentsAt(1)));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       ChangePasswordFormIsFilledAutomatically) {
  GURL main_url("https://example.com/");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->StartPasswordChange(main_url, u"test", u"pa$$word",
                                                 WebContents());
  // Activate tab with password change to simplify testing.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(1));

  PasswordsNavigationObserver observer(WebContents());
  EXPECT_TRUE(observer.Wait());

  // Wait and verify the old password is filled correctly.
  WaitForElementValue("password", "pa$$word");

  // Verify there is a new password generated and it's filled into both fields.
  std::string new_password =
      GetElementValue(/*iframe_id=*/"null", "new_password_1");
  EXPECT_FALSE(new_password.empty());
  CheckElementValue("new_password_2", new_password);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, PasswordChangeStateUpdated) {
  MockPasswordChangeDelegateObserver observer;

  GURL main_url("https://example.com/");
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));
  password_change_service()->StartPasswordChange(main_url, u"test", u"pa$$word",
                                                 WebContents());

  // Verify the delegate is created and it's currently waiting for change
  // password form.
  auto* delegate = password_change_service()->GetPasswordChangeDelegate(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(delegate);
  delegate->AddObserver(&observer);
  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());

  // Verify observer is invoked when the state changes.
  EXPECT_CALL(observer,
              OnStateChanged(PasswordChangeDelegate::State::kChangingPassword));

  // Activate tab with password change to simplify testing.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(1));
  PasswordsNavigationObserver navigation_observer(WebContents());
  EXPECT_TRUE(navigation_observer.Wait());

  // Wait and verify the old password is filled correctly.
  WaitForElementValue("password", "pa$$word");

  delegate->RemoveObserver(&observer);
}

// TODO(crbug.com/382703186): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       DISABLED_GeneratedPasswordIsPreSaved) {
  GURL main_url("https://example.com/");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->StartPasswordChange(main_url, u"test", u"pa$$word",
                                                 WebContents());
  // Activate tab with password change to simplify testing.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(1));

  PasswordsNavigationObserver observer(WebContents());
  EXPECT_TRUE(observer.Wait());
  WaitForElementValue("password", "pa$$word");

  // Verify generated password is pre-saved.
  WaitForPasswordStore();
  CheckThatCredentialsStored(
      /*username=*/"", GetElementValue(/*iframe_id=*/"null", "new_password_1"));
}

// Verify that after password change is stopped, password change delegate is not
// returned.
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, StopPasswordChange) {
  GURL main_url("https://example.com/");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(
          embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->StartPasswordChange(main_url, u"test", u"pa$$word",
                                                 WebContents());

  auto* password_change_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(password_change_service()->GetPasswordChangeDelegate(
      password_change_tab));

  password_change_service()
      ->GetPasswordChangeDelegate(password_change_tab)
      ->Stop();

  EXPECT_FALSE(password_change_service()->GetPasswordChangeDelegate(
      password_change_tab));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, NewPasswordIsSaved) {
  GURL main_url("https://example.com/");
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->StartPasswordChange(main_url, u"test", u"pa$$word",
                                                 WebContents());
  // Activate tab with password change to simplify testing.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(1));

  PasswordsNavigationObserver password_change_page_observer(WebContents());
  EXPECT_TRUE(password_change_page_observer.Wait());

  WaitForElementValue("password", "pa$$word");
  std::string new_password =
      GetElementValue(/*iframe_id=*/"null", "new_password_1");

  // Emulate a navigation as an indication of successful submission.
  PasswordsNavigationObserver new_page_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/password/done.html")));
  EXPECT_TRUE(new_page_observer.Wait());

  // Verify generated password is saved.
  WaitForPasswordStore();
  CheckThatCredentialsStored("test", new_password);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OldPasswordIsUpdated) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  GURL origin = embedded_test_server()->GetURL("example.com", "/");
  password_manager::PasswordForm form;
  form.signon_realm = origin.spec();
  form.url = origin;
  form.username_value = u"test";
  form.password_value = u"pa$$word";
  password_store->AddLogin(form);
  WaitForPasswordStore();

  GURL main_url("https://example.com/");
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "example.com", "/password/update_form_empty_fields.html")));

  password_change_service()->StartPasswordChange(
      main_url, form.username_value, form.password_value, WebContents());
  // Activate tab with password change to simplify testing.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(1));

  PasswordsNavigationObserver password_change_page_observer(WebContents());
  EXPECT_TRUE(password_change_page_observer.Wait());
  WaitForElementValue("password", "pa$$word");

  std::string new_password =
      GetElementValue(/*iframe_id=*/"null", "new_password_1");

  // Emulate a navigation as an indication of successful submission.
  PasswordsNavigationObserver new_page_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/password/done.html")));
  EXPECT_TRUE(new_page_observer.Wait());

  // Verify saved password is updated.
  WaitForPasswordStore();
  CheckThatCredentialsStored(base::UTF16ToUTF8(form.username_value),
                             new_password);
}
