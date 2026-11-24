// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"

#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_ui_test_utils.h"
#endif

namespace ttc {

TtcCoreBrowserTestBase::TtcCoreBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

TtcCoreBrowserTestBase::~TtcCoreBrowserTestBase() = default;

void TtcCoreBrowserTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_ANDROID)
  // Test devices are often offline. The offline indicator that appears a
  // couple of seconds after startup changes the height of the top controls
  // and resizes the page's view in the middle of a test. That asserts if the
  // controls are not fully shown at that moment (e.g. during tab fullscreen or
  // a tab switch, see crbug.com/574927722), and it changes sizes that tests
  // compare against.
  command_line->AppendSwitch(switches::kForceOnlineConnectionStateForIndicator);
#endif
}

void TtcCoreBrowserTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  PlatformBrowserTest::SetUpBrowserContextKeyedServices(context);

  // Replace the service with one whose sessions use a MockConversation.
  TtcKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<TtcKeyedService>(
            Profile::FromBrowserContext(context),
            base::BindRepeating(&TtcCoreBrowserTestBase::MakeMockConversation));
      }));
}

void TtcCoreBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
}

Profile* TtcCoreBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* TtcCoreBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

tabs::TabInterface* TtcCoreBrowserTestBase::active_tab() {
  return tabs::TabInterface::GetFromContents(web_contents());
}

TabListInterface* TtcCoreBrowserTestBase::tab_list() {
  return TabListInterface::From(active_tab()->GetBrowserWindowInterface());
}

TtcKeyedService& TtcCoreBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

void TtcCoreBrowserTestBase::OpenForegroundTab(const GURL& url) {
  tabs::TabInterface* tab =
      tab_list()->OpenTab(url, /*index=*/-1, /*foreground=*/true);
  ASSERT_TRUE(tab);
#if BUILDFLAG(IS_ANDROID)
  // On phones the new tab animation selects the new tab again when it ends.
  // Wait for it, so that it can't undo a subsequent tab switch.
  android_ui_test_utils::WaitForBrowsingLayout(
      tab->GetBrowserWindowInterface());
#endif
  ASSERT_TRUE(content::WaitForLoadStop(tab->GetContents()));
  ASSERT_TRUE(base::test::RunUntil([tab]() { return tab->IsActivated(); }));
}

MockConversation* TtcCoreBrowserTestBase::conversation() {
  auto* session_controller =
      static_cast<SessionControllerImpl*>(ttc_service().session_controller());
  if (!session_controller) {
    return nullptr;
  }
  return static_cast<MockConversation*>(&session_controller->conversation());
}

// static
std::unique_ptr<Conversation> TtcCoreBrowserTestBase::MakeMockConversation(
    SessionController&,
    std::unique_ptr<AudioController>) {
  return std::make_unique<testing::NiceMock<MockConversation>>();
}

}  // namespace ttc
