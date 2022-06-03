// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSIONS_BROWSERTEST_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSIONS_BROWSERTEST_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
class MockPermissionPromptFactory;
}  // namespace permissions

// This is a base class for end-to-end testing of features that have
// permissions. It will navigate to the URL passed in upon construction, ready
// to execute javascript to test the permissions. There are also a set of common
// test functions that are provided (see the functions with the Common* prefix).
// These should be called to ensure basic test coverage of the feature's
// permission that is being added. Custom tests can also be added for the
// permission. For an example of how to use this base class, see
// FlashPermissionBrowserTest.
class PermissionsBrowserTest : public InProcessBrowserTest {
 public:
  explicit PermissionsBrowserTest(const std::string& test_url);
  ~PermissionsBrowserTest() override;

  // InProcessBrowserTest
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  virtual void TriggerPrompt() = 0;
  virtual bool FeatureUsageSucceeds() = 0;

  std::string test_url() const { return test_url_; }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

 protected:
  bool RunScriptReturnBool(const std::string& script);

  content::WebContents* GetWebContents();

  // Common tests that should be called by subclasses.
  void CommonFailsBeforeRequesting();
  void CommonFailsIfDismissed();
  void CommonFailsIfBlocked();
  void CommonSucceedsIfAllowed();

 private:
  std::string test_url_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSIONS_BROWSERTEST_H_
