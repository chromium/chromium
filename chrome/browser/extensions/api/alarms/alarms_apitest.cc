// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/state_store.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using extensions::ResultCatcher;

class AlarmsApiTest : public ExtensionApiTest {
 public:
  AlarmsApiTest() = default;
  ~AlarmsApiTest() override = default;
  AlarmsApiTest& operator=(const AlarmsApiTest&) = delete;
  AlarmsApiTest(const AlarmsApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  static base::ListValue BuildEventArguments(bool last_message) {
    api::test::OnMessage::Info info;
    info.data = "";
    info.last_message = last_message;
    return api::test::OnMessage::Create(info);
  }

  const Extension* LoadAlarmsExtensionIncognito(const char* path) {
    return LoadExtension(test_data_dir_.AppendASCII("alarms").AppendASCII(path),
                         {.allow_in_incognito = true});
  }

  void FlushStateStore(Profile* profile) {
    base::RunLoop run_loop;
    extensions::ExtensionSystem::Get(profile)->state_store()->FlushForTesting(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }
};

// Tests that an alarm created by an extension with incognito split mode is
// only triggered in the browser context it was created in.
IN_PROC_BROWSER_TEST_F(AlarmsApiTest, IncognitoSplit) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(incognito_profile);
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  EventRouter* event_router = EventRouter::Get(incognito_profile);

  ExtensionTestMessageListener listener("ready: false");

  ExtensionTestMessageListener listener_incognito("ready: true");

  ASSERT_TRUE(LoadAlarmsExtensionIncognito("split"));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // Open incognito window.
  PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));

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
  catcher.RestrictToBrowserContext(profile());

  ASSERT_TRUE(LoadAlarmsExtensionIncognito("spanning"));

  // Open incognito window.
  PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/451193827): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Count DISABLED_Count
#else
#define MAYBE_Count Count
#endif
IN_PROC_BROWSER_TEST_F(AlarmsApiTest, MAYBE_Count) {
  EXPECT_TRUE(RunExtensionTest("alarms/count")) << message_;
}

class AlarmsNameHistogramTest : public ExtensionApiTest {
 public:
  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ExtensionApiTest::SetUp();
  }

  void TearDown() override {
    histogram_tester_ = nullptr;
    ExtensionApiTest::TearDown();
  }

 protected:
  int GetAlarmCountForExtension() {
    const Extension* extension = GetSingleLoadedExtension();
    EXPECT_TRUE(extension) << "Exactly one extension should be loaded.";
    EXPECT_EQ("Alarm Name Test Extension", extension->name());
    AlarmManager* alarm_manager = AlarmManager::Get(profile());
    EXPECT_TRUE(alarm_manager) << "AlarmManager should be loaded.";
    return alarm_manager->GetCountForExtension(extension->id());
  }

  void WaitForStateStore() {
    base::RunLoop run_loop;
    ExtensionSystem::Get(profile())->state_store()->FlushForTesting(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test that the histogram for determining maximum alarm name lengths
// counts the length of the longest alarm.
IN_PROC_BROWSER_TEST_F(AlarmsNameHistogramTest, PRE_Name) {
  // Write the alarms into persistent storage to be read in the main
  // part of the test.
  EXPECT_TRUE(RunExtensionTest("alarms/name")) << message_;

  // Ensure that extension had set exactly three alarms.
  WaitForStateStore();
  ASSERT_EQ(3, GetAlarmCountForExtension());
}

// Confirm the alarms are loaded from persistent storage and histogram is
// emitted on extension load.
IN_PROC_BROWSER_TEST_F(AlarmsNameHistogramTest, Name) {
  // Wait until extension alarms are loaded.
  WaitForStateStore();

  // Ensure that extension had set exactly three alarms (as set in
  // background.js).
  ASSERT_EQ(3, GetAlarmCountForExtension());

  // After the alarms had been loaded, the histogram counts should have
  // been incremented.
  histogram_tester_->ExpectUniqueSample(
      // 6 is the index of the expected bucket, AlarmNameLength::k126_250.
      "Extensions.AlarmManager.AlarmsMaxNameLength", 6,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(AlarmsApiTest, PRE_AlarmPersistence) {
  // Create both alarms and verify both exist.
  SetCustomArg("create");
  ResultCatcher catcher;

  ChromeTestExtensionLoader loader(profile());
  // Pack the extension to ensure a reload is not treated as a re-install, which
  // would clear out the alarms.
  loader.set_pack_extension(true);
  const Extension* extension =
      loader.LoadExtension(test_data_dir_.AppendASCII("alarms/persistence"))
          .get();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Verify that after an extension reload both still exist.
  SetCustomArg("verify_after_reload");
  ReloadExtension(extension->id());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Flush the state store to ensure the modified state is correctly stored
  // on-disk (which could otherwise be potentially racy).
  FlushStateStore(profile());
}

IN_PROC_BROWSER_TEST_F(AlarmsApiTest, AlarmPersistence) {
  ExtensionSystem* extension_system = ExtensionSystem::Get(profile());

  // Wait until the extension system is ready.
  base::RunLoop run_loop;
  extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // This runs after a real browser restart.
  // Verifies that only persistent alarm exists.
  SetCustomArg("verify_after_restart");

  // Reload the extension to ensure it runs with the right custom arg and
  // we catch the result.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  ReloadExtension(extension->id());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
