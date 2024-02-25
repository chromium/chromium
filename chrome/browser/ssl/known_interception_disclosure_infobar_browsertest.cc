// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/crl_set.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace {

size_t GetInfobarCount(content::WebContents* contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  return infobar_manager ? infobar_manager->infobars().size() : 0;
}

infobars::InfoBar* GetInfobar(content::WebContents* contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  DCHECK(infobar_manager);
  return infobar_manager->infobars()[0];
}

// Follows same logic as clicking the "Continue" button would.
void CloseInfobar(content::WebContents* contents) {
  infobars::InfoBar* infobar = GetInfobar(contents);
  if (!infobar)
    return;

  ASSERT_TRUE(
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Accept());
  infobar->RemoveSelf();
}

}  // namespace

class KnownInterceptionDisclosureInfobarTest : public InProcessBrowserTest {
 public:
  KnownInterceptionDisclosureInfobarTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  KnownInterceptionDisclosureInfobarTest(
      const KnownInterceptionDisclosureInfobarTest&) = delete;
  KnownInterceptionDisclosureInfobarTest& operator=(
      const KnownInterceptionDisclosureInfobarTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_server_.Start());

    // Load a CRLSet that marks the root as a known MITM.
    std::string crl_set_bytes;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                                 "crlset_known_interception_by_root.raw"),
                             &crl_set_bytes);
    }
    base::RunLoop run_loop;
    content::GetCertVerifierServiceFactory()->UpdateCRLSet(
        base::as_bytes(base::make_span(crl_set_bytes)), run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       OnlyShowDisclosureOncePerSession) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* tab1 = tab_strip_model->GetActiveWebContents();

  auto* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  KnownInterceptionDisclosureCooldown::GetInstance()->SetClockForTesting(
      std::unique_ptr<base::Clock>(clock));

  // Trigger the disclosure infobar by navigating to a page served by a root
  // marked as known interception.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(1u, GetInfobarCount(tab1));

  // Test that the infobar is shown on new tabs after it has been triggered
  // once.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* tab2 = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1u, GetInfobarCount(tab2));

  // Close the new tab.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_USER_GESTURE);

  // Reload the first page -- infobar should still show.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(1u, GetInfobarCount(tab1));

  // Dismiss the infobar.
  CloseInfobar(tab1);
  EXPECT_EQ(0u, GetInfobarCount(tab1));

  // Try to trigger again by reloading the page -- infobar should not show.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(0u, GetInfobarCount(tab1));

  // Move clock ahead 8 days.
  clock->Advance(base::Days(8));

  // Trigger the infobar again -- infobar should show again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(1u, GetInfobarCount(tab1));
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       PRE_CooldownResetsOnBrowserRestartDesktop) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // Trigger the disclosure infobar.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(1u, GetInfobarCount(tab));

  // Dismiss the infobar.
  CloseInfobar(tab);
  EXPECT_EQ(0u, GetInfobarCount(tab));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CooldownResetsOnBrowserRestartDesktop \
  DISABLED_CooldownResetsOnBrowserRestartDesktop
#else
#define MAYBE_CooldownResetsOnBrowserRestartDesktop \
  CooldownResetsOnBrowserRestartDesktop
#endif
IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosureInfobarTest,
                       MAYBE_CooldownResetsOnBrowserRestartDesktop) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // On restart, no infobar should be shown initially.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetInfobarCount(tab));

  // Triggering the disclosure infobar again after browser restart should show
  // the infobar (the cooldown period should no longer apply on Desktop).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInterceptedUrl));
  EXPECT_EQ(1u, GetInfobarCount(tab));
}
