// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

using testing::_;
using testing::NiceMock;
using testing::Return;

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  MOCK_METHOD(bool,
              CheckBrowseUrl,
              (const GURL&,
               const safe_browsing::SBThreatTypeSet&,
               Client*,
               safe_browsing::CheckBrowseUrlType),
              (override));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

std::optional<base::Value> RunGetReferrerChainFunction(
    content::BrowserContext* browser_context,
    int tab_id) {
  scoped_refptr<SafeBrowsingPrivateGetReferrerChainFunction> function(
      base::MakeRefCounted<SafeBrowsingPrivateGetReferrerChainFunction>());
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), "[" + base::NumberToString(tab_id) + "]",
          browser_context);
  return value;
}

}  // namespace

class SafeBrowsingPrivateApiBrowserTest : public InProcessBrowserTest {
 public:
  SafeBrowsingPrivateApiBrowserTest() = default;
  ~SafeBrowsingPrivateApiBrowserTest() override = default;

  SafeBrowsingPrivateApiBrowserTest(const SafeBrowsingPrivateApiBrowserTest&) =
      delete;
  SafeBrowsingPrivateApiBrowserTest& operator=(
      const SafeBrowsingPrivateApiBrowserTest&) = delete;

 private:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    sb_service_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();

    mock_db_manager_ =
        base::MakeRefCounted<NiceMock<MockSafeBrowsingDatabaseManager>>();
    ON_CALL(*mock_db_manager_, CheckBrowseUrl(_, _, _, _))
        .WillByDefault(Return(true));
    sb_service_factory_->SetTestDatabaseManager(mock_db_manager_.get());

    safe_browsing::SafeBrowsingService::RegisterFactory(
        sb_service_factory_.get());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    Profile* profile = Profile::FromBrowserContext(context);
    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext,
                NiceMock<password_manager::MockPasswordStoreInterface>>));

    AccountPasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile,
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext,
                NiceMock<password_manager::MockPasswordStoreInterface>>));
  }

  void TearDownInProcessBrowserTestFixture() override {
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      sb_service_factory_;
  scoped_refptr<NiceMock<MockSafeBrowsingDatabaseManager>> mock_db_manager_;
};

// Tests that chrome.safeBrowsingPrivate.getReferrerChain returns a result.
// Actual contents tested with util unit test.
// TODO(livvielin): Look into simulating navigation event so that we can test
// the size of the result.
IN_PROC_BROWSER_TEST_F(SafeBrowsingPrivateApiBrowserTest, GetReferrerChain) {
  const std::vector<GURL> urls = {
      embedded_test_server()->GetURL("foo.test", "/title1.html"),
      embedded_test_server()->GetURL("bar.test", "/title2.html")};
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (const auto& url : urls) {
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  }

  ASSERT_TRUE(sessions::SessionTabHelper::FromWebContents(web_contents));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  std::optional<base::Value> referrer_chain =
      RunGetReferrerChainFunction(browser()->profile(), tab_id);
  ASSERT_TRUE(referrer_chain);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingPrivateApiBrowserTest,
                       GetReferrerChainForNonSafeBrowsingUser) {
  // Disable Safe Browsing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  const std::vector<GURL> urls = {
      embedded_test_server()->GetURL("foo.test", "/title1.html"),
      embedded_test_server()->GetURL("bar.test", "/title2.html")};
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (const auto& url : urls) {
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  }

  ASSERT_TRUE(sessions::SessionTabHelper::FromWebContents(web_contents));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  std::optional<base::Value> referrer_chain =
      RunGetReferrerChainFunction(browser()->profile(), tab_id);
  ASSERT_FALSE(referrer_chain);
}

}  // namespace extensions
