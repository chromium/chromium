// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/crl_set.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/ssl/known_interception_disclosure_message_delegate.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
messages::MessageWrapper* g_last_message_wrapper = nullptr;

size_t GetDisclosureCount(content::WebContents* contents) {
  return g_last_message_wrapper ? 1 : 0;
}

void CloseDisclosure(content::WebContents* contents) {
  if (g_last_message_wrapper) {
    g_last_message_wrapper->HandleDismissCallback(
        base::android::AttachCurrentThread(),
        static_cast<int>(messages::DismissReason::UNKNOWN));
    g_last_message_wrapper = nullptr;
  }
}
#else
size_t GetDisclosureCount(content::WebContents* contents) {
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
void CloseDisclosure(content::WebContents* contents) {
  infobars::InfoBar* infobar = GetInfobar(contents);
  if (!infobar) {
    return;
  }

  ASSERT_TRUE(
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->Accept());
  infobar->RemoveSelf();
}
#endif

}  // namespace

class KnownInterceptionDisclosurePlatformBrowserTest
    : public PlatformBrowserTest {
 public:
  KnownInterceptionDisclosurePlatformBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  KnownInterceptionDisclosurePlatformBrowserTest(
      const KnownInterceptionDisclosurePlatformBrowserTest&) = delete;
  KnownInterceptionDisclosurePlatformBrowserTest& operator=(
      const KnownInterceptionDisclosurePlatformBrowserTest&) = delete;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &mock_message_dispatcher_bridge_);

    ON_CALL(mock_message_dispatcher_bridge_, EnqueueMessage)
        .WillByDefault([](messages::MessageWrapper* message,
                          content::WebContents* web_contents,
                          messages::MessageScopeType scope_type,
                          messages::MessagePriority priority) {
          g_last_message_wrapper = message;
          return true;
        });
#endif
    PlatformBrowserTest::SetUp();
  }

  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
    g_last_message_wrapper = nullptr;
#endif
    PlatformBrowserTest::TearDown();
  }

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
        base::as_byte_span(crl_set_bytes), run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
#if BUILDFLAG(IS_ANDROID)
  testing::NiceMock<messages::MockMessageDispatcherBridge>
      mock_message_dispatcher_bridge_;
#endif
};

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosurePlatformBrowserTest,
                       DisclosureTriggerSmokeTest) {
#if BUILDFLAG(IS_ANDROID)
  // Clear the mock so the real MessageDispatcherBridge is used to ensure
  // the Java-side UI is correctly triggered.
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
#endif

  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);

  // Trigger showing the disclosure.
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosurePlatformBrowserTest,
                       OnlyShowDisclosureOncePerSession) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  content::WebContents* tab1 = chrome_test_utils::GetActiveWebContents(this);

  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  clock_ptr->SetNow(base::Time::Now());
  KnownInterceptionDisclosureCooldown::GetInstance()->SetClockForTesting(
      std::move(clock));

  // Trigger the disclosure infobar by navigating to a page served by a root
  // marked as known interception.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));

#if !BUILDFLAG(IS_ANDROID)
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // Test that the infobar is shown on new tabs after it has been triggered
  // once.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* tab2 = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1u, GetDisclosureCount(tab2));

  // Close the new tab.
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_USER_GESTURE);
#endif

  // Reload the first page -- infobar should still show.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));

  // Dismiss the disclosure.
  CloseDisclosure(tab1);
  EXPECT_EQ(0u, GetDisclosureCount(tab1));

  // Try to trigger again by reloading the page -- disclosure should not show.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(0u, GetDisclosureCount(tab1));

  // Move clock ahead 8 days.
  clock_ptr->Advance(base::Days(8));

  // Trigger the disclosure again -- disclosure should show again.
  ASSERT_TRUE(content::NavigateToURL(tab1, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab1));
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosurePlatformBrowserTest,
                       PRE_CooldownResetsOnBrowserRestart) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // Trigger the disclosure.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab));

  // Dismiss the disclosure.
  CloseDisclosure(tab);
  EXPECT_EQ(0u, GetDisclosureCount(tab));

#if BUILDFLAG(IS_ANDROID)
  // Ensure the pref is written to disk so it can be read in the next test.
  base::RunLoop run_loop;
  Profile::FromBrowserContext(tab->GetBrowserContext())
      ->GetPrefs()
      ->CommitPendingWrite(run_loop.QuitClosure());
  run_loop.Run();
#endif
}

IN_PROC_BROWSER_TEST_F(KnownInterceptionDisclosurePlatformBrowserTest,
                       CooldownResetsOnBrowserRestart) {
  const GURL kInterceptedUrl(https_server_.GetURL("/ssl/google.html"));

  // On restart, no disclosure should be shown initially.
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_EQ(0u, GetDisclosureCount(tab));

#if !BUILDFLAG(IS_ANDROID)
  // Triggering the disclosure again after browser restart should show
  // the infobar (the cooldown period should no longer apply on Desktop).
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
  EXPECT_EQ(1u, GetDisclosureCount(tab));
#else
  // On Android, the cooldown persists across restarts, so the disclosure should
  // NOT show.
  ASSERT_TRUE(content::NavigateToURL(tab, kInterceptedUrl));
  EXPECT_EQ(0u, GetDisclosureCount(tab));
#endif
}
