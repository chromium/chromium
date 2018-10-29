// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_view.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/simple_web_view_dialog.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "ui/views/controls/webview/webview.h"

namespace chromeos {

namespace {

class StubDelegate : public CaptivePortalWindowProxyDelegate {
 public:
  StubDelegate() {}
  ~StubDelegate() override {}
  void OnPortalDetected() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StubDelegate);
};

class InterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  explicit InterstitialPageDelegate(content::WebContents* web_contents) {
    content::InterstitialPage* page = content::InterstitialPage::Create(
        web_contents, true, GURL("http://foo"), this);
    page->Show();
  }

  ~InterstitialPageDelegate() override {}

 private:
  // InterstitialPageDelegate implementation:
  std::string GetHTMLContents() override { return "HTML Contents"; }

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageDelegate);
};

}  // namespace

class SimpleWebViewDialogTest : public LoginManagerTest {
 public:
  SimpleWebViewDialogTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~SimpleWebViewDialogTest() override {}

  InterstitialPageDelegate* CreateDelegate(CaptivePortalWindowProxy* proxy) {
    SimpleWebViewDialog* dialog = proxy->captive_portal_view_for_testing();
    CHECK(dialog) << "CaptivePortalView is not initialized";
    return new InterstitialPageDelegate(dialog->web_view_->web_contents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleWebViewDialogTest);
};

IN_PROC_BROWSER_TEST_F(SimpleWebViewDialogTest, Interstitial) {
  content::WebContents* web_contents =
      LoginDisplayHost::default_host()->GetOobeWebContents();
  StubDelegate delegate;
  CaptivePortalWindowProxy proxy(&delegate, web_contents);
  proxy.Show();

  // Delegate creates a page and passes himself to it. Page owns the
  // delegate and will be destroyed by the end of the test.
  CreateDelegate(&proxy);
}

}  // namespace chromeos
