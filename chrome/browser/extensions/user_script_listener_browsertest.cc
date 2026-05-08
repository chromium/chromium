// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using UserScriptListenerTest = ExtensionBrowserTest;

// Test that navigations block while waiting for content scripts to load.
IN_PROC_BROWSER_TEST_F(UserScriptListenerTest,
                       NavigationWaitsForContentScriptsToLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestingProfile profile;
  ExtensionsBrowserClient::Get()
      ->GetUserScriptListener()
      ->SetUserScriptsNotReadyForTesting(&profile);

  content::WebContents* web_contents = GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  content::DidStartNavigationObserver start_observer(web_contents);

  GURL url = embedded_test_server()->GetURL("/echo");
  web_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  start_observer.Wait();
  ASSERT_TRUE(start_observer.navigation_handle());
  EXPECT_TRUE(start_observer.navigation_handle()->IsDeferredForTesting());

  ExtensionsBrowserClient::Get()
      ->GetUserScriptListener()
      ->TriggerUserScriptsReadyForTesting(&profile);

  nav_observer.Wait();
}

}  // namespace extensions
