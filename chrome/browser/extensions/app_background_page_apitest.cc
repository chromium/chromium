// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_contents_service_observer.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "components/embedder_support/switches.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_process_host.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

using extensions::Extension;

namespace {

class BackgroundContentsCreationObserver
    : public BackgroundContentsServiceObserver {
 public:
  explicit BackgroundContentsCreationObserver(Profile* profile) {
    observation_.Observe(
        BackgroundContentsServiceFactory::GetForProfile(profile));
  }

  BackgroundContentsCreationObserver(
      const BackgroundContentsCreationObserver&) = delete;
  BackgroundContentsCreationObserver& operator=(
      const BackgroundContentsCreationObserver&) = delete;

  ~BackgroundContentsCreationObserver() override = default;

  void OnBackgroundContentsOpened(
      const BackgroundContentsOpenedDetails& details) override {
    ++opens_;
  }

  int opens() const { return opens_; }

 private:
  // The number of background contents that have been opened since creation.
  int opens_ = 0;

  base::ScopedObservation<BackgroundContentsService,
                          BackgroundContentsServiceObserver>
      observation_{this};
};

class BackgroundContentsClosedObserver
    : protected BackgroundContentsServiceObserver {
 public:
  explicit BackgroundContentsClosedObserver(Profile* profile) {
    observation_.Observe(
        BackgroundContentsServiceFactory::GetForProfile(profile));
  }

  BackgroundContentsClosedObserver(const BackgroundContentsClosedObserver&) =
      delete;
  BackgroundContentsClosedObserver& operator=(
      const BackgroundContentsClosedObserver&) = delete;

  ~BackgroundContentsClosedObserver() override = default;

  void Wait() { run_loop_.Run(); }

 protected:
  void OnBackgroundContentsClosed() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;

  base::ScopedObservation<BackgroundContentsService,
                          BackgroundContentsServiceObserver>
      observation_{this};
};

}  // namespace

class AppBackgroundPageApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
    command_line->AppendSwitch(extensions::switches::kAllowHTTPBackgroundPage);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  bool CreateApp(const std::string& app_manifest,
                 base::FilePath* app_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!app_dir_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Unable to create a temporary directory.";
      return false;
    }
    base::FilePath manifest_path =
        app_dir_.GetPath().AppendASCII("manifest.json");
    if (!base::WriteFile(manifest_path, app_manifest)) {
      LOG(ERROR) << "Unable to write manifest to file.";
      return false;
    }
    *app_dir = app_dir_.GetPath();
    return true;
  }

  bool VerifyBackgroundMode(bool expected_background_mode) {
    BackgroundModeManager* manager =
        g_browser_process->background_mode_manager();
    // If background mode is disabled on this platform (e.g. cros), then skip
    // this check.
    if (!manager || !manager->IsBackgroundModePrefEnabled()) {
      DLOG(WARNING) << "Skipping check - background mode disabled";
      return true;
    }

    return manager->IsBackgroundModeActive() == expected_background_mode;
  }

  void UnloadExtensionViaTask(const std::string& id) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AppBackgroundPageApiTest::UnloadExtension,
                                  base::Unretained(this), id));
  }

 private:
  base::ScopedTempDir app_dir_;
};

namespace {

// Fixture to assist in testing v2 app background pages containing
// Native Client embeds.
class AppBackgroundPageNaClTest : public AppBackgroundPageApiTest {
 public:
  AppBackgroundPageNaClTest() : extension_(nullptr) {
    feature_list_.InitAndEnableFeature(kNaclAllow);
  }
  ~AppBackgroundPageNaClTest() override {}

  void SetUpOnMainThread() override {
    AppBackgroundPageApiTest::SetUpOnMainThread();
    extensions::ProcessManager::SetEventPageIdleTimeForTesting(1000);
    extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1000);
  }

  const Extension* extension() { return extension_; }

 protected:
  void LaunchTestingApp() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath app_dir;
    base::PathService::Get(chrome::DIR_GEN_TEST_DATA, &app_dir);
    app_dir = app_dir.AppendASCII(
        "ppapi/tests/extensions/background_keepalive/newlib");
    extension_ = LoadExtension(app_dir);
    ASSERT_TRUE(extension_);
  }

 private:
  raw_ptr<const Extension> extension_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// This test is meaningless if background mode is not enabled.
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, Basic) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  BackgroundContentsClosedObserver closed_observer(browser()->profile());
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode should not be active until a background page is created.
  ASSERT_TRUE(VerifyBackgroundMode(false));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic")) << message_;
  // The test closes the background contents, so we should fall back to no
  // background mode at the end.
  closed_observer.Wait();
  EXPECT_TRUE(VerifyBackgroundMode(false));
}
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, LacksPermission) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  }"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/lacks_permission"))
      << message_;
  ASSERT_TRUE(VerifyBackgroundMode(false));
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, ManifestBackgroundPage) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/test.html\""
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  // Background mode should not be active now because no background app was
  // loaded.
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode be active now because a background page was created when
  // the app was loaded.
  ASSERT_TRUE(VerifyBackgroundMode(true));

  // Verify that the background contents exist.
  const Extension* extension = GetSingleLoadedExtension();
  BackgroundContents* background_contents =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents((extension->id()));
  ASSERT_TRUE(background_contents);

  // Verify that window.opener in the background contents is not set when
  // creating the background page through the manifest (not through
  // window.open).
  EXPECT_FALSE(background_contents->web_contents()->GetOpener());
  EXPECT_EQ(true, content::EvalJs(background_contents->web_contents(),
                                  "window.opener == null;"));

  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, NoJsBackgroundPage) {
  // Keep the task manager up through this test to verify that a crash doesn't
  // happen when window.open creates a background page that switches
  // RenderViewHosts. See http://crbug.com/165138.
  chrome::ShowTaskManager(browser());
  BackgroundContentsCreationObserver creation_observer(browser()->profile());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/test.html\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"allow_js_access\": false"
      "  }"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));

  // There isn't a background page loaded initially.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents(extension->id()));
  // The test makes sure that window.open returns null.
  ASSERT_TRUE(RunExtensionTest("app_background_page/no_js")) << message_;
  // And after it runs there should be a background page.
  BackgroundContents* background_contents =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents((extension->id()));
  ASSERT_TRUE(background_contents);

  // Verify that window.opener in the background contents is not set when
  // allow_js_access=false.
  EXPECT_FALSE(background_contents->web_contents()->GetOpener());
  EXPECT_EQ(true, content::EvalJs(background_contents->web_contents(),
                                  "window.opener == null;"));

  // Verify multiple BackgroundContents don't get opened despite multiple
  // window.open calls.
  EXPECT_EQ(1, creation_observer.opens());
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, NoJsManifestBackgroundPage) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/bg.html\","
      "    \"allow_js_access\": false"
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));

  // The background page should load.
  const Extension* extension = GetSingleLoadedExtension();
  BackgroundContents* background_contents =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents((extension->id()));
  ASSERT_TRUE(background_contents);

  // Verify that window.opener in the background contents is not set when
  // creating the background page through the manifest (not through
  // window.open).
  EXPECT_FALSE(background_contents->web_contents()->GetOpener());
  EXPECT_EQ(true, content::EvalJs(background_contents->web_contents(),
                                  "window.opener == null;"));

  // window.open should return null.
  ASSERT_TRUE(RunExtensionTest("app_background_page/no_js_manifest")) <<
      message_;

  // Verify that window.opener in the background contents is still not set.
  EXPECT_FALSE(background_contents->web_contents()->GetOpener());
  EXPECT_EQ(true, content::EvalJs(background_contents->web_contents(),
                                  "window.opener == null;"));

  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, OpenTwoBackgroundPages) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(RunExtensionTest("app_background_page/two_pages")) << message_;
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, OpenTwoPagesWithManifest) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/bg.html\""
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(RunExtensionTest("app_background_page/two_with_manifest")) <<
      message_;
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, OpenPopupFromBGPage) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"background\": { \"page\": \"http://a.com:%u/extensions/api_test/"
      "app_background_page/bg_open/bg_open_bg.html\" },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/bg_open")) << message_;
}

// Partly a regression test for crbug.com/756465. Namely, that window.open
// correctly matches an app URL with a path component.
// Flaky on Chrome OS https://crbug.com/1462141.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OpenThenClose DISABLED_OpenThenClose
#else
#define MAYBE_OpenThenClose OpenThenClose
#endif
IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, MAYBE_OpenThenClose) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/extensions/api_test\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/extensions/api_test\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  // There isn't a background page loaded initially.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents(extension->id()));
  // Background mode should not be active until a background page is created.
  ASSERT_TRUE(VerifyBackgroundMode(false));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic_open")) << message_;
  // Background mode should be active now because a background page was created.
  ASSERT_TRUE(VerifyBackgroundMode(true));

  // Verify that the background contents exist.
  BackgroundContents* background_contents =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents((extension->id()));
  ASSERT_TRUE(background_contents);

  // Verify that window.opener in the background contents is set.
  content::RenderFrameHost* background_opener =
      background_contents->web_contents()->GetOpener();
  ASSERT_TRUE(background_opener);
  std::string window_opener_href =
      content::EvalJs(background_contents->web_contents(),
                      "window.opener.location.href;")
          .ExtractString();
  EXPECT_EQ(window_opener_href,
            background_opener->GetLastCommittedURL().spec());

  // Now close the BackgroundContents.
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic_close")) << message_;

  // Background mode should no longer be active.
  ASSERT_TRUE(VerifyBackgroundMode(false));
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents(extension->id()));
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, UnloadExtensionWhileHidden) {
  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/test.html\""
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  // Background mode should not be active now because no background app was
  // loaded.
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode be active now because a background page was created when
  // the app was loaded.
  ASSERT_TRUE(VerifyBackgroundMode(true));

  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())
          ->GetAppBackgroundContents(extension->id()));

  // Close all browsers - app should continue running.
  set_exit_when_last_browser_closes(false);
  CloseBrowserSynchronously(browser());

  // Post a task to unload the extension - this should cause Chrome to exit
  // cleanly (not crash).
  UnloadExtensionViaTask(extension->id());
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(VerifyBackgroundMode(false));
}

#if BUILDFLAG(ENABLE_NACL)

// Verify that active NaCl embeds raise the keepalive count.
IN_PROC_BROWSER_TEST_F(AppBackgroundPageNaClTest, BackgroundKeepaliveActive) {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  LaunchTestingApp();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  const auto api_activity = std::make_pair(extensions::Activity::API_FUNCTION,
                                           std::string("test.sendMessage"));
  const auto pepper_api_activity =
      std::make_pair(extensions::Activity::PEPPER_API, std::string());
  // When the app calls chrome.test.sendMessage() the keepalive count stays
  // incremented until the call completes (i.e. until we call Reply() below).
  // So between WaitUntilSatisfied() and Reply(), we know that the count must
  // be in the incremented state, and in this case that is the only
  // contributor to the keepalive count.
  EXPECT_EQ(1, manager->GetLazyKeepaliveCount(extension()));
  extensions::ProcessManager::ActivitiesMultiset activities =
      manager->GetLazyKeepaliveActivities(extension());
  EXPECT_THAT(activities, testing::UnorderedElementsAre(api_activity));

  ExtensionTestMessageListener created1_listener("created_module:1",
                                                 ReplyBehavior::kWillReply);
  ready_listener.Reply("create_module");
  EXPECT_TRUE(created1_listener.WaitUntilSatisfied());

  // Now chrome.test.sendMessage() is incrementing the keepalive count, but
  // there is also a Native Client module active, incrementing it again.
  EXPECT_EQ(2, manager->GetLazyKeepaliveCount(extension()));
  activities = manager->GetLazyKeepaliveActivities(extension());
  EXPECT_THAT(activities,
              testing::UnorderedElementsAre(api_activity, pepper_api_activity));

  ExtensionTestMessageListener created2_listener("created_module:2",
                                                 ReplyBehavior::kWillReply);
  created1_listener.Reply("create_module");
  EXPECT_TRUE(created2_listener.WaitUntilSatisfied());

  // Keepalive comes from chrome.test.sendMessage, plus two modules.
  EXPECT_EQ(3, manager->GetLazyKeepaliveCount(extension()));
  activities = manager->GetLazyKeepaliveActivities(extension());
  EXPECT_EQ(3u, activities.size());
  EXPECT_THAT(activities,
              testing::UnorderedElementsAre(api_activity, pepper_api_activity,
                                            pepper_api_activity));

  // Tear-down both modules.
  ExtensionTestMessageListener destroyed1_listener("destroyed_module",
                                                   ReplyBehavior::kWillReply);
  created2_listener.Reply("destroy_module");
  EXPECT_TRUE(destroyed1_listener.WaitUntilSatisfied());
  ExtensionTestMessageListener destroyed2_listener("destroyed_module");
  destroyed1_listener.Reply("destroy_module");
  EXPECT_TRUE(destroyed2_listener.WaitUntilSatisfied());

  // Both modules are gone, and no sendMessage API reply is pending (since the
  // last listener has the |will_reply| flag set to |false|).
  EXPECT_EQ(0, manager->GetLazyKeepaliveCount(extension()));
  activities = manager->GetLazyKeepaliveActivities(extension());
  EXPECT_TRUE(activities.empty());
}

#endif  //  BUILDFLAG(ENABLE_NACL)
