// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/idle/idle_polling_service.h"
#include "ui/base/idle/idle_time_provider.h"
#include "ui/base/test/idle_test_utils.h"

using ::testing::NiceMock;

namespace {

class MockIdleTimeProvider : public ui::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;

  MockIdleTimeProvider(const MockIdleTimeProvider&) = delete;
  MockIdleTimeProvider& operator=(const MockIdleTimeProvider&) = delete;

  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());
};

class IdleBrowserTest : public InProcessBrowserTest {
 public:
  IdleBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~IdleBrowserTest() override = default;

  void SetUp() override {
    // Prevent user education from polling idle state.
    UserEducationServiceFactory::GetInstance()
        ->disable_idle_polling_for_testing();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server()->Start());
    // The default 15s polling interval causes tests to time out.
    ui::IdlePollingService::GetInstance()->SetPollIntervalForTest(
        base::Seconds(1));
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void TestSubframePermissionPolicy(bool positive_test) {
    GURL subframe_url = https_server()->GetURL("b.com", "/simple_page.html");
    GURL url = https_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(" + subframe_url.spec() +
                     (positive_test ? "{allow-idle-detection})" : ")"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::IDLE_DETECTION, CONTENT_SETTING_ASK);

    auto* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);

    std::string script = "IdleDetector.requestPermission();";

    content::RenderFrameHost* child =
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

    EXPECT_EQ(positive_test ? "granted" : "denied", EvalJs(child, script));
  }

 private:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(IdleBrowserTest, Start) {
  GURL url = https_server()->GetURL("a.com", "/simple_page.html");
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::IDLE_DETECTION, CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Test that statuses are updated after idleDetector.start().
  std::string script = R"(
    (async () => {
        let idleDetector = new IdleDetector();
        let promise = new Promise(function(resolve) {
          let states = [];
          idleDetector.addEventListener('change', e => {
            states.push(`${idleDetector.userState}-${idleDetector.screenState}`)
            if (states.length >= 3) {
              let states_str = states.join(',');
              resolve(states_str);
            }
          });
        });
        await idleDetector.start();
        return promise;
    }) ();
  )";

  auto mock_time_provider = std::make_unique<NiceMock<MockIdleTimeProvider>>();

  EXPECT_CALL(*mock_time_provider, CalculateIdleTime())
      // Simulates a user going idle.
      .WillOnce(testing::Return(base::Seconds(60)))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(base::Seconds(60)))
      // Simulates a user going back to active.
      .WillRepeatedly(testing::Return(base::Seconds(0)));

  EXPECT_CALL(*mock_time_provider, CheckIdleStateIsLocked())
      // Simulates unlocked screen while user goes idle.
      .WillOnce(testing::Return(false))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(true))
      // Simulates an unlocked screen as user goes back to active.
      .WillRepeatedly(testing::Return(false));

  ui::test::ScopedIdleProviderForTest scoped_idle_provider(
      std::move(mock_time_provider));

  EXPECT_EQ("idle-unlocked,idle-locked,active-unlocked",
            EvalJs(web_contents(), script));
}

IN_PROC_BROWSER_TEST_F(IdleBrowserTest, SubframeWithoutPolicy) {
  TestSubframePermissionPolicy(false);
}

IN_PROC_BROWSER_TEST_F(IdleBrowserTest, SubframeWithPolicy) {
  TestSubframePermissionPolicy(true);
}

}  // namespace
