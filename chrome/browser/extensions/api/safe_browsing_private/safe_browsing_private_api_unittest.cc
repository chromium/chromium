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
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

namespace {

using testing::NiceMock;

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

// Creates WebContents with |urls| as history.
std::unique_ptr<content::WebContents> CreateWebContentsWithHistory(
    Profile* profile,
    const std::vector<GURL>& urls) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile, nullptr);

  for (const auto& url : urls) {
    web_contents->GetController().LoadURL(
        url, content::Referrer(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK), std::string());

    content::RenderFrameHostTester::CommitPendingLoad(
        &web_contents->GetController());
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
    EXPECT_EQ(url, web_contents->GetVisibleURL());
  }

  return web_contents;
}

}  // namespace

class SafeBrowsingPrivateApiUnitTest : public ExtensionServiceTestBase {
 public:
  SafeBrowsingPrivateApiUnitTest(SafeBrowsingPrivateApiUnitTest&) = delete;
  SafeBrowsingPrivateApiUnitTest& operator=(SafeBrowsingPrivateApiUnitTest&) =
      delete;

 protected:
  SafeBrowsingPrivateApiUnitTest() = default;
  ~SafeBrowsingPrivateApiUnitTest() override = default;

  Browser* browser() { return browser_.get(); }

 private:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
};

void SafeBrowsingPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_ = std::unique_ptr<Browser>(Browser::Create(params));

  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &password_manager::BuildPasswordStoreInterface<
              content::BrowserContext,
              NiceMock<password_manager::MockPasswordStoreInterface>>));

  AccountPasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &password_manager::BuildPasswordStoreInterface<
              content::BrowserContext,
              NiceMock<password_manager::MockPasswordStoreInterface>>));

  // Initialize Safe Browsing service.
  safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
  auto* safe_browsing_service = sb_service_factory.CreateSafeBrowsingService();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
      safe_browsing_service);
  g_browser_process->safe_browsing_service()->Initialize();
  safe_browsing_service->OnProfileAdded(profile());
}

void SafeBrowsingPrivateApiUnitTest::TearDown() {
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  browser_window_.reset();

  // Make sure the NetworkContext owned by SafeBrowsingService is destructed
  // before the NetworkService object..
  TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);

  // Depends on LocalState from ChromeRenderViewHostTestHarness.
  if (SystemNetworkContextManager::GetInstance())
    SystemNetworkContextManager::DeleteInstance();

  ExtensionServiceTestBase::TearDown();
}

// Tests that chrome.safeBrowsingPrivate.getReferrerChain returns a result.
// Actual contents tested with util unit test.
// TODO(livvielin): Look into simulating navigation event so that we can test
// the size of the result.
TEST_F(SafeBrowsingPrivateApiUnitTest, GetReferrerChain) {
  const std::vector<GURL> urls = {GURL("http://www.foo.test"),
                                  GURL("http://www.bar.test")};
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContentsWithHistory(profile(), urls);
  content::WebContents* raw_web_contents = web_contents.get();
  ASSERT_TRUE(raw_web_contents);

  CreateSessionServiceTabHelper(raw_web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_web_contents).id();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);

  std::optional<base::Value> referrer_chain =
      RunGetReferrerChainFunction(profile(), tab_id);
  ASSERT_TRUE(referrer_chain);
}

TEST_F(SafeBrowsingPrivateApiUnitTest, GetReferrerChainForNonSafeBrowsingUser) {
  // Disable Safe Browsing.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  const std::vector<GURL> urls = {GURL("http://www.foo.test"),
                                  GURL("http://www.bar.test")};
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContentsWithHistory(profile(), urls);
  content::WebContents* raw_web_contents = web_contents.get();
  ASSERT_TRUE(raw_web_contents);

  CreateSessionServiceTabHelper(raw_web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_web_contents).id();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);

  std::optional<base::Value> referrer_chain =
      RunGetReferrerChainFunction(profile(), tab_id);
  ASSERT_FALSE(referrer_chain);
}

}  // namespace extensions
