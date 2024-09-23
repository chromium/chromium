// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/origin.h"

constexpr int kTitleResourceId = IDS_USB_DEVICE_CHOOSER_PROMPT;

class CreateChooserTitlePopUpBrowserTest
    : public PopupTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    base::FilePath server_root(FILE_PATH_LITERAL("chrome/test/data"));
    server_->AddDefaultHandlers(server_root);
    CHECK(server_->Start());
    PopupTestBase::SetUp();
  }

  net::EmbeddedTestServer* server() { return server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CreateChooserTitlePopUpBrowserTest);

IN_PROC_BROWSER_TEST_F(CreateChooserTitlePopUpBrowserTest,
                       UseOriginNotUrl) {
  ASSERT_TRUE(
      NavigateToURL(browser()->tab_strip_model()->GetActiveWebContents(),
                    server()->GetURL("/simple.html")));

  std::string script("open('', '_blank', 'popup,fullscreen')");
  Browser* popup_browser = OpenPopup(browser(), script);
  ASSERT_TRUE(popup_browser);
  content::RenderFrameHost* rfh = popup_browser->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  EXPECT_EQ(rfh->GetLastCommittedOrigin(), server()->GetOrigin());
  EXPECT_TRUE(rfh->GetLastCommittedURL().IsAboutBlank());

  std::u16string expected_string =
      base::StrCat({url_formatter::FormatOriginForSecurityDisplay(
                        server()->GetOrigin(),
                        url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
                    u" wants to connect"});
  EXPECT_EQ(expected_string, CreateChooserTitle(rfh, kTitleResourceId));
}
