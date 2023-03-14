// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class FakeClientSideDetectionService : public ClientSideDetectionService {
 public:
  FakeClientSideDetectionService()
      : ClientSideDetectionService(nullptr, nullptr, nullptr) {}

  void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token) override {
    saved_request_ = *verdict;
    saved_callback_ = std::move(callback);
    access_token_ = access_token;
    request_callback_.Run();
  }

  const ClientPhishingRequest& saved_request() { return saved_request_; }

  bool saved_callback_is_null() { return saved_callback_.is_null(); }

  ClientReportPhishingRequestCallback saved_callback() {
    return std::move(saved_callback_);
  }

  void SetModel(const ClientSideModel& model) { model_ = model; }

  CSDModelType GetModelType() override { return CSDModelType::kProtobuf; }

  const std::string& GetModelStr() override {
    client_side_model_ = model_.SerializeAsString();
    return client_side_model_;
  }

  void SetRequestCallback(const base::RepeatingClosure& closure) {
    request_callback_ = closure;
  }

  base::WeakPtr<ClientSideDetectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Override to always pass pre-classification checks for policy test
  bool IsPrivateIPAddress(const net::IPAddress& address) const override {
    return false;
  }

  // Override to always pass pre-classification checks for policy test
  bool IsLocalResource(const net::IPAddress& address) const override {
    return false;
  }

 private:
  ClientPhishingRequest saved_request_;
  ClientReportPhishingRequestCallback saved_callback_;
  ClientSideModel model_;
  std::string access_token_;
  std::string client_side_model_;
  base::RepeatingClosure request_callback_;
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)) {}

  MockSafeBrowsingUIManager(const MockSafeBrowsingUIManager&) = delete;
  MockSafeBrowsingUIManager& operator=(const MockSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

 protected:
  ~MockSafeBrowsingUIManager() override = default;
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            content::GetUIThreadTaskRunner({}),
            content::GetIOThreadTaskRunner({})) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD2(CheckCsdAllowlistUrl, AsyncMatch(const GURL&, Client*));

  // Override to silence not implemented warnings.
  bool CanCheckUrl(const GURL& url) const override { return true; }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

}  // namespace

class ClientSideDetectionHostPrerenderBrowserTest
    : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ClientSideDetectionHostPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~ClientSideDetectionHostPrerenderBrowserTest() override = default;
  ClientSideDetectionHostPrerenderBrowserTest(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;
  ClientSideDetectionHostPrerenderBrowserTest& operator=(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    model_.set_version(123);
    model_.set_max_words_per_term(1);
    // This model will always trigger.
    model_.set_threshold_probability(-1);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ClientSideModel& client_side_model() { return model_; }

 private:
  ClientSideModel model_;
  content::test::PrerenderTestHelper prerender_helper_;
};

class ClientSideDetectionHostPolicyBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ClientSideDetectionHostPolicyBrowserTest() = default;
  ~ClientSideDetectionHostPolicyBrowserTest() override = default;
  ClientSideDetectionHostPolicyBrowserTest(
      const ClientSideDetectionHostPolicyBrowserTest&) = delete;
  ClientSideDetectionHostPolicyBrowserTest& operator=(
      const ClientSideDetectionHostPolicyBrowserTest&) = delete;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    model_.set_version(123);
    model_.set_max_words_per_term(1);
    // This model will always trigger.
    model_.set_threshold_probability(-1);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetPolicy() {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy, GetParam());
  }

  ClientSideModel& client_side_model() { return model_; }

 private:
  ClientSideModel model_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       PrerenderShouldNotAffectClientSideDetection) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  // A prerendered navigation committing should not cancel classification.
  // We simulate the commit of a prerendered navigation to avoid races
  // between the completion of phishing detection in the primary
  // main frame's renderer and the commit of a real prerendered navigation.
  // TODO(mcnee): Use a real prerendered navigation here and make sure the
  // navigation doesn't race with the classification.
  content::MockNavigationHandle prerendered_navigation_handle;
  prerendered_navigation_handle.set_has_committed(true);
  prerendered_navigation_handle.set_is_in_primary_main_frame(false);
  csd_host->DidFinishNavigation(&prerendered_navigation_handle);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback()).Run(page_url, true);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       ClassifyPrerenderedPageAfterActivation) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());

  fake_csd_service.SendModelToRenderers();

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Prerender then activate a phishing page.
  const GURL prerender_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  prerender_helper().AddPrerender(prerender_url);
  prerender_helper().NavigatePrimaryPage(prerender_url);

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback()).Run(prerender_url, true);
}

INSTANTIATE_TEST_SUITE_P(,
                         ClientSideDetectionHostPolicyBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ClientSideDetectionHostPolicyBrowserTest,
                       PolicyEnabled) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    GTEST_SKIP();
  }

  // Set CSD-Phishing policy value for test
  SetPolicy();

  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());
  fake_csd_service.SendModelToRenderers();

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();
  scoped_refptr<StrictMock<MockSafeBrowsingDatabaseManager>>
      mock_database_manager = new StrictMock<MockSafeBrowsingDatabaseManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(fake_csd_service.GetWeakPtr());
  csd_host->set_ui_manager(mock_ui_manager.get());
  csd_host->set_database_manager(mock_database_manager.get());

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());
  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));

  if (GetParam()) {
    // If policy is enabled, pre-classification checks should use
    // CheckCsdAllowlistUrl. Override CheckCsdAllowlistUrl to allow sending
    // phishing request.
    EXPECT_CALL(*mock_database_manager, CheckCsdAllowlistUrl(page_url, _))
        .WillOnce(Return(AsyncMatch::NO_MATCH));
  } else {
    // If policy is disabled, pre-classification check should fail before
    // CheckCsdAllowlistUrl.
    EXPECT_CALL(*mock_database_manager, CheckCsdAllowlistUrl(page_url, _))
        .Times(0);
  }

  // Navigate to malicious page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  if (GetParam()) {
    run_loop.Run();
    ASSERT_FALSE(fake_csd_service.saved_callback_is_null());
    EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);

    // Expect an interstitial to be shown.
    EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
    std::move(fake_csd_service.saved_callback()).Run(page_url, true);
  } else {
    ASSERT_TRUE(fake_csd_service.saved_callback_is_null());
  }
}
}  // namespace safe_browsing
