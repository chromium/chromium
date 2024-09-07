// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ssl/chrome_security_state_model_delegate.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/security_state/content/android/security_state_client.h"
#include "components/security_state/content/security_state_tab_helper.h"
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

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  auto* security_state_client = security_state::GetSecurityStateClient();
  ASSERT_TRUE(security_state_client);
  {
    base::RunLoop run_loop;
    helper->set_get_security_level_callback_for_tests_(run_loop.QuitClosure());
    EXPECT_EQ(helper->GetSecurityLevel(),
              security_state_client->MaybeCreateSecurityStateModelDelegate()
                  ->GetSecurityLevel(contents));

    // The test won't finish until SecurityStateTabHelper::GetSecurityLevel()
    // is called.
    run_loop.Run();
  }
}
