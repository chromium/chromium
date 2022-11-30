// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permissions_browsertest.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

PermissionsBrowserTest::PermissionsBrowserTest(const std::string& test_url)
    : test_url_(test_url) {}

PermissionsBrowserTest::~PermissionsBrowserTest() = default;

void PermissionsBrowserTest::SetUpOnMainThread() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  prompt_factory_ =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(test_url())));
}

void PermissionsBrowserTest::TearDownOnMainThread() {
  prompt_factory_.reset();
}

bool PermissionsBrowserTest::RunScriptReturnBool(const std::string& script) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents()->GetPrimaryMainFrame(), script, &result));
  return result;
}

content::WebContents* PermissionsBrowserTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void PermissionsBrowserTest::CommonFailsBeforeRequesting() {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  // Dismiss any prompts if they are shown when using the feature.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DISMISS);
  EXPECT_FALSE(FeatureUsageSucceeds());
}

void PermissionsBrowserTest::CommonFailsIfDismissed() {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DISMISS);
  TriggerPrompt();

  EXPECT_FALSE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

void PermissionsBrowserTest::CommonFailsIfBlocked() {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);
  TriggerPrompt();

  EXPECT_FALSE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

void PermissionsBrowserTest::CommonSucceedsIfAllowed() {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  TriggerPrompt();

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}
