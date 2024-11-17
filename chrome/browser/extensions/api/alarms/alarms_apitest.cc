// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_platform_apitest.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#endif

namespace extensions {

#if !BUILDFLAG(IS_ANDROID)

using extensions::ResultCatcher;

class AlarmsApiTest : public ExtensionApiTest {
 public:
  AlarmsApiTest() = default;
  ~AlarmsApiTest() override = default;
  AlarmsApiTest& operator=(const AlarmsApiTest&) = delete;
  AlarmsApiTest(const AlarmsApiTest&) = delete;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ExtensionApiTest::SetUp();
  }

  void TearDown() override {
    histogram_tester_.release();
    ExtensionApiTest::TearDown();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  static base::Value::List BuildEventArguments(bool last_message) {
    api::test::OnMessage::Info info;
    info.data = "";
    info.last_message = last_message;
    return api::test::OnMessage::Create(info);
  }

  const Extension* LoadAlarmsExtensionIncognito(const char* path) {
    return LoadExtension(test_data_dir_.AppendASCII("alarms").AppendASCII(path),
                         {.allow_in_incognito = true});
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that an alarm created by an extension with incognito split mode is
// only triggered in the browser context it was created in.
IN_PROC_BROWSER_TEST_F(AlarmsApiTest, IncognitoSplit) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(incognito_profile);
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EventRouter* event_router = EventRouter::Get(incognito_profile);

  ExtensionTestMessageListener listener("ready: false");

  ExtensionTestMessageListener listener_incognito("ready: true");

  ASSERT_TRUE(LoadAlarmsExtensionIncognito("split"));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // Open incognito window.
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  event_router->BroadcastEvent(std::make_unique<Event>(
      events::FOR_TEST, api::test::OnMessage::kEventName,
      BuildEventArguments(true)));

  EXPECT_TRUE(catcher.GetNextResult());
  EXPECT_TRUE(catcher_incognito.GetNextResult());
}

// Tests that the behavior for an alarm created in incognito context should be
// the same if incognito is in spanning mode.
IN_PROC_BROWSER_TEST_F(AlarmsApiTest, IncognitoSpanning) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(LoadAlarmsExtensionIncognito("spanning"));

  // Open incognito window.
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
using AlarmsPlatformApiTest = ExtensionPlatformApiTest;
#else
using AlarmsPlatformApiTest = ExtensionApiTest;
#endif

IN_PROC_BROWSER_TEST_F(AlarmsPlatformApiTest, Count) {
  EXPECT_TRUE(RunExtensionTest("alarms/count")) << message_;
}

}  // namespace extensions
