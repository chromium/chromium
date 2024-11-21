// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_controller.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
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

std::unique_ptr<KeyedService> CreateTestAffiliationService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockAffiliationService>>();
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

  MockAffiliationService* affiliation_service() {
    return static_cast<MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(browser()->profile()));
  }

  ChromePasswordChangeService* password_change_service() {
    return PasswordChangeServiceFactory::GetForProfile(browser()->profile());
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<PasswordChangeBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       StartingPasswordChangeOpensNewTab) {
  // Assert that there is a single tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  GURL main_url("https://example.com/"),
      change_pwd_url("https://example.com/password/");
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(change_pwd_url));

  password_change_service()->StartPasswordChange(main_url, u"test", u"password",
                                                 WebContents());

  // Verify a new tab is added while the focus remained on the initial tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Verify a new tab is opened with a change pwd url.
  EXPECT_EQ(change_pwd_url,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}
