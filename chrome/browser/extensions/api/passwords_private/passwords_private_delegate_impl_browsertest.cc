// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <memory>
#include <string>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kUsername[] = "Bob";
constexpr char kUrl[] = "https://www.example.com";

class PasswordsPrivateDelegateImplBrowserTest : public InProcessBrowserTest {
 public:
  PasswordsPrivateDelegateImplBrowserTest() {
    // Enable the unified side panel, as this is a prerequisite for the
    // Automated Password Change flow to be startable.
    feature_list.InitAndEnableFeature(features::kUnifiedSidePanel);
  }
  ~PasswordsPrivateDelegateImplBrowserTest() override = default;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplBrowserTest,
                       StartAutomatedPasswordChange) {
  PasswordsPrivateDelegateImpl delegate(browser()->profile());

  const GURL url(kUrl);
  api::passwords_private::PasswordUiEntry credential;
  credential.username = kUsername;
  credential.change_password_url = std::make_unique<std::string>(kUrl);
  base::MockCallback<
      PasswordsPrivateDelegate::StartAutomatedPasswordChangeCallback>
      apc_callback;

  content::TestNavigationObserver navigation_observer(url);
  navigation_observer.StartWatchingNewWebContents();

  delegate.StartAutomatedPasswordChange(credential, apc_callback.Get());
  navigation_observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);

  // The `ApcClient` is running.
  ApcClient* apc_client = ApcClient::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(apc_client);
  EXPECT_TRUE(apc_client->IsRunning());

  EXPECT_CALL(apc_callback, Run(false)).Times(1);
  apc_client->Stop();
}

}  // namespace

}  // namespace extensions
