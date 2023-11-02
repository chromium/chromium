// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

#include <memory>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_apc_scrim_manager.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace {

class ApcExternalActionDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ApcExternalActionDelegateBrowserTest() = default;
  ~ApcExternalActionDelegateBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Test support.
  MockApcScrimManager mock_apc_scrim_manager_;
  MockAssistantDisplayDelegate mock_assistant_display_delegate_;
  autofill_assistant::MockWebsiteLoginManager mock_website_login_manager_;
};

IN_PROC_BROWSER_TEST_F(ApcExternalActionDelegateBrowserTest,
                       OpenPasswordManager) {
  auto delegate = std::make_unique<ApcExternalActionDelegate>(
      web_contents(), &mock_assistant_display_delegate_,
      &mock_apc_scrim_manager_, &mock_website_login_manager_);

  {
    content::LoadStopObserver observer(web_contents());
    delegate->OpenPasswordManager();
    observer.Wait();
  }

  EXPECT_EQ(web_contents()->GetURL(),
            GURL(base::StrCat({chrome::kChromeUISettingsURL,
                               chrome::kPasswordManagerSubPage})));
}

}  // namespace
