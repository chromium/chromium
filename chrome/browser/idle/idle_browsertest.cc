// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/idle_test_utils.h"

#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;

namespace {

class MockIdleTimeProvider : public content::IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleTimeProvider);
};

class IdleBrowserTest : public InProcessBrowserTest {
 public:
  IdleBrowserTest() = default;
  ~IdleBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "IdleDetection");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(IdleBrowserTest, Start) {
  GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::IDLE_DETECTION, CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), url);

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
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(60)))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(60)))
      // Simulates a user going back to active.
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

  EXPECT_CALL(*mock_time_provider, CheckIdleStateIsLocked())
      // Simulates unlocked screen while user goes idle.
      .WillOnce(testing::Return(false))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(true))
      // Simulates an unlocked screen as user goes back to active.
      .WillRepeatedly(testing::Return(false));

  content::IdleManagerHelper::SetIdleTimeProviderForTest(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      std::move(mock_time_provider));

  std::string result =
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script)
          .ExtractString();
  std::vector<std::string> states = base::SplitString(
      result, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  EXPECT_EQ("idle-unlocked", states.at(0));
  EXPECT_EQ("idle-locked", states.at(1));
  EXPECT_EQ("active-unlocked", states.at(2));
}

}  // namespace
