// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::WebContents;
using extensions::ResultCatcher;

namespace extensions {

class IncognitoApiTest : public ExtensionApiTest,
                         public testing::WithParamInterface<bool> {
 public:
  IncognitoApiTest() {
    scoped_feature_list_.InitWithFeatureState(
        enterprise_isolated_mode::kEnableEnterpriseIsolatedMode,
        IsEnterpriseIsolatedMode());
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ExtensionApiTest::SetUpBrowserContextKeyedServices(context);
    if (IsEnterpriseIsolatedMode()) {
      Profile::FromBrowserContext(context)->GetPrefs()->SetInteger(
          enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
          static_cast<int>(
              enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
    }
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  bool IsEnterpriseIsolatedMode() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    IncognitoApiTest,
    testing::Bool(),
    [](const testing::TestParamInfo<IncognitoApiTest::ParamType>& info) {
      return info.param ? "IsolatedMode" : "Incognito";
    });

IN_PROC_BROWSER_TEST_P(IncognitoApiTest, IncognitoNoScript) {
  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("incognito")
      .AppendASCII("content_scripts")));

  // Open incognito window and navigate to test page.
  GURL test_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  WebContents* tab = PlatformOpenURLOffTheRecord(profile(), test_url);

  // Verify the script didn't run.
  EXPECT_EQ(true, content::EvalJs(tab, "document.title == 'Unmodified'"));
}

IN_PROC_BROWSER_TEST_P(IncognitoApiTest, IncognitoYesScript) {
  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("content_scripts"),
      {.allow_in_incognito = true}));

  // Open incognito window and navigate to test page.
  GURL test_url = embedded_test_server()->GetURL("/extensions/test_file.html");
  WebContents* tab = PlatformOpenURLOffTheRecord(profile(), test_url);

  // Verify the script ran.
  EXPECT_EQ(true, content::EvalJs(tab, "document.title == 'modified'"));
}

IN_PROC_BROWSER_TEST_P(IncognitoApiTest, NoCrashWithMultipleExtensions) {
  // Load a dummy extension. This just tests that we don't regress a
  // crash fix when multiple incognito- and non-incognito-enabled extensions
  // are mixed.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("content_scripts").AppendASCII("inject_div")));

  // Load an incognito extension.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("content_scripts"),
      {.allow_in_incognito = true}));

  // Dummy extension #2.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("content_scripts")
                                .AppendASCII("css_injection")));

  // No crash.
}

// Tests that when listeners in the `incognito` service worker are not removed
// when the `regular` service worker stops.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest, IncognitoSplitKeepListener) {
  constexpr char kEvent[] = "tabs.onCreated";

  // Prepare a test extension.
  TestExtensionDir test_dir;
  constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "background": {
             "service_worker": "background.js"
           },
           "incognito": "split",
           "permissions": ["tabs"]
         })";
  test_dir.WriteManifest(kManifest);
  constexpr char kBackgroundJs[] =
      R"(
        chrome.tabs.onCreated.addListener(() => {});

        self.addEventListener('install', e => e.waitUntil(skipWaiting()));
        self.addEventListener('activate', e => {
          chrome.test.sendMessage(
              chrome.extension.inIncognitoContext ? "waiting_incognito"
                                                  : "waiting");
        });
      )";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ExtensionTestMessageListener listener("waiting", ReplyBehavior::kWontReply);
  ExtensionTestMessageListener listener_incognito("waiting_incognito",
                                                  ReplyBehavior::kWontReply);

  PlatformOpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  // Waits for both `regular` and `incognito` instances.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  EventRouter* event_router = EventRouter::Get(profile());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return event_router->ExtensionHasEventListener(extension->id(), kEvent);
  }));

  // Stops the `regular` service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // The `incognito` service worker should have active listeners.
  EXPECT_TRUE(event_router->HasNonLazyEventListenerForTesting(kEvent));
}

// Tests that an extension which is enabled for incognito mode doesn't
// accidentally create an incognito profile.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest, DontCreateIncognitoProfile) {
  ASSERT_FALSE(profile()->HasPrimaryOTRProfile());
  ASSERT_TRUE(RunExtensionTest("incognito/dont_create_profile", {},
                               {.allow_in_incognito = true}))
      << message_;
  ASSERT_FALSE(profile()->HasPrimaryOTRProfile());
}

IN_PROC_BROWSER_TEST_P(IncognitoApiTest, Incognito) {
  ResultCatcher catcher;

  // Open incognito window and navigate to test page.
  PlatformOpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("incognito").AppendASCII("apis"),
                    {.allow_in_incognito = true}));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that the APIs in an incognito-enabled split-mode extension work
// properly.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest, IncognitoSplitMode) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  ExtensionTestMessageListener listener("waiting", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito("waiting_incognito",
                                                  ReplyBehavior::kWillReply);

  // Open incognito window and navigate to test page.
  PlatformOpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("split"),
      {.allow_in_incognito = true}));

  // Wait for both extensions to be ready before telling them to proceed.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());
  listener.Reply("go");
  listener_incognito.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Tests that the APIs in an incognito-disabled extension don't see incognito
// events or callbacks.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest, IncognitoDisabled) {
  ResultCatcher catcher;
  ExtensionTestMessageListener listener("createIncognitoTab",
                                        ReplyBehavior::kWillReply);

  // Open incognito window and navigate to test page.
  PlatformOpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("incognito").AppendASCII("apis_disabled")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));
  listener.Reply("created");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Verifies that cookies set in a split-mode OTR extension context are not
// persisted.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest,
                       ExtensionCookiesDoNotPersistAfterOTRSession) {
  static constexpr char kManifest[] = R"({
    "name": "Split Mode Extension Cookies Test",
    "version": "1.0",
    "manifest_version": 3,
    "incognito": "split"
  })";

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(kManifest);
  test_extension_dir.WriteFile(FILE_PATH_LITERAL("empty.html"), "");

  const Extension* extension = LoadExtension(test_extension_dir.UnpackedPath(),
                                             {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  const GURL extension_url = extension->GetResourceURL("empty.html");

  // Set a persistent cookie in the OTR extension page, then close the OTR tab
  // and wait for the OTR profile to be destroyed.
  WebContents* off_the_record_contents =
      PlatformOpenURLOffTheRecord(profile(), extension_url);
  ASSERT_TRUE(
      content::ExecJs(off_the_record_contents,
                      "document.cookie = 'otr_cookie=1; max-age=3600'"));
  EXPECT_EQ("otr_cookie=1",
            content::EvalJs(off_the_record_contents, "document.cookie"));
  ProfileDestructionWaiter profile_destruction_waiter(
      Profile::FromBrowserContext(
          off_the_record_contents->GetBrowserContext()));
  CloseTabForWebContents(off_the_record_contents);
  profile_destruction_waiter.Wait();

  // A new OTR session must not inherit the cookie from the previous session.
  WebContents* new_off_the_record_contents =
      PlatformOpenURLOffTheRecord(profile(), extension_url);
  EXPECT_EQ("",
            content::EvalJs(new_off_the_record_contents, "document.cookie"));

  // The regular profile must also not see the OTR cookie.
  WebContents* regular_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(regular_contents, extension_url));
  EXPECT_EQ("", content::EvalJs(regular_contents, "document.cookie"));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Test that opening a popup from an incognito browser window works properly.
// http://crbug.com/40304461.
IN_PROC_BROWSER_TEST_P(IncognitoApiTest, DISABLED_IncognitoPopup) {
  ResultCatcher catcher;

  const extensions::Extension* const extension = LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("popup"),
      {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  // Open incognito window and navigate to test page.
  BrowserWindowInterface* incognito_browser = OpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  // Simulate the incognito's browser action being clicked.
  ExtensionActionTestHelper::Create(incognito_browser)->Press(extension->id());

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
#endif

}  // namespace extensions
