// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ssl/chrome_security_state_model_delegate.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/security_state/content/android/security_state_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class ChromeSecurityStateClientTest : public AndroidBrowserTest {
 public:
  ChromeSecurityStateClientTest() = default;
  ~ChromeSecurityStateClientTest() override = default;

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateClientTest,
                       CorrectSecurityStatModelDelegateCreated) {
  content::WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  auto* security_state_client = security_state::GetSecurityStateClient();
  ASSERT_TRUE(security_state_client);
  std::unique_ptr<SecurityStateModelDelegate> delegate =
      security_state_client->MaybeCreateSecurityStateModelDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(chrome_security_state::GetSecurityLevel(contents),
            delegate->GetSecurityLevel(contents));
  EXPECT_EQ(chrome_security_state::GetMaliciousContentStatus(contents),
            delegate->GetMaliciousContentStatus(contents));
}
