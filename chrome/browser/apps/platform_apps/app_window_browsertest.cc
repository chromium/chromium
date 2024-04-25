// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window_geometry_cache.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/display/display_switches.h"

using extensions::AppWindowGeometryCache;
using extensions::ResultCatcher;

// This helper class can be used to wait for changes in the app window
// geometry cache registry for a specific window in a specific extension.
class GeometryCacheChangeHelper : AppWindowGeometryCache::Observer {
 public:
  GeometryCacheChangeHelper(AppWindowGeometryCache* cache,
                            const extensions::ExtensionId& extension_id,
                            const std::string& window_id,
                            const gfx::Rect& bounds)
      : cache_(cache),
        extension_id_(extension_id),
        window_id_(window_id),
        bounds_(bounds),
        satisfied_(false),
        waiting_(false) {
    cache_->AddObserver(this);
  }

  // This method will block until the app window geometry cache registry will
  // provide a bound for |window_id_| that is entirely different (as in x/y/w/h)
  // from the initial |bounds_|.
  void WaitForEntirelyChanged() {
    if (satisfied_)
      return;

    waiting_ = true;
    loop_.Run();
  }

  // Implements the AppWindowGeometryCache::Observer interface.
  void OnGeometryCacheChanged(const extensions::ExtensionId& extension_id,
                              const std::string& window_id,
                              const gfx::Rect& bounds) override {
    if (extension_id != extension_id_ || window_id != window_id_)
      return;

    if (bounds_.x() != bounds.x() && bounds_.y() != bounds.y() &&
        bounds_.width() != bounds.width() &&
        bounds_.height() != bounds.height()) {
      satisfied_ = true;
      cache_->RemoveObserver(this);

      if (waiting_)
        loop_.QuitWhenIdle();
    }
  }

 private:
  raw_ptr<AppWindowGeometryCache> cache_;
  extensions::ExtensionId extension_id_;
  std::string window_id_;
  gfx::Rect bounds_;
  bool satisfied_;
  bool waiting_;
  // base::RunLoop used to require kNestableTaskAllowed
  base::RunLoop loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

// Helper class for tests related to the Apps Window API (chrome.app.window).
class AppWindowAPITest : public extensions::PlatformAppBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "1.0");
  }

  bool RunAppWindowAPITest(const char* testName) {
    if (!BeginAppWindowAPITest(testName))
      return false;

    ResultCatcher catcher;
    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

  bool RunAppWindowAPITestAndWaitForRoundTrip(const char* testName) {
    if (!BeginAppWindowAPITest(testName))
      return false;

    ExtensionTestMessageListener round_trip_listener("WaitForRoundTrip",
                                                     ReplyBehavior::kWillReply);
    if (!round_trip_listener.WaitUntilSatisfied()) {
      message_ = "Did not get the 'WaitForRoundTrip' message.";
      return false;
    }

    round_trip_listener.Reply("");

    ResultCatcher catcher;
    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

 private:
  bool BeginAppWindowAPITest(const char* testName) {
    ExtensionTestMessageListener launched_listener("Launched",
                                                   ReplyBehavior::kWillReply);
    LoadAndLaunchPlatformApp("window_api", &launched_listener);
    if (!launched_listener.WaitUntilSatisfied()) {
      message_ = "Did not get the 'Launched' message.";
      return false;
    }

    launched_listener.Reply(testName);
    return true;
  }
};

// These tests are flaky after https://codereview.chromium.org/57433010/.
// See http://crbug.com/319613.

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestCreate) {
  ASSERT_TRUE(RunAppWindowAPITest("testCreate")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestSingleton) {
  ASSERT_TRUE(RunAppWindowAPITest("testSingleton")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestCloseEvent) {
  ASSERT_TRUE(RunAppWindowAPITest("testCloseEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestMaximize) {
  ASSERT_TRUE(RunAppWindowAPITest("testMaximize")) << message_;
}

// Flaky on Linux. http://crbug.com/424399.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestMinimize DISABLED_TestMinimize
#else
#define MAYBE_TestMinimize TestMinimize
#endif

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestMinimize) {
  ASSERT_TRUE(RunAppWindowAPITest("testMinimize")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestRestore) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestore")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestRestoreAfterClose) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestoreAfterClose")) << message_;
}

// These tests will be flaky in Linux as window bounds change asynchronously.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestDeprecatedBounds DISABLED_TestDeprecatedBounds
#define MAYBE_TestInitialBounds DISABLED_TestInitialBounds
#define MAYBE_TestInitialConstraints DISABLED_TestInitialConstraints
#define MAYBE_TestSetBounds DISABLED_TestSetBounds
#define MAYBE_TestSetSizeConstraints DISABLED_TestSetSizeConstraints
#else
#define MAYBE_TestDeprecatedBounds TestDeprecatedBounds
#define MAYBE_TestInitialBounds TestInitialBounds
#define MAYBE_TestInitialConstraints TestInitialConstraints
#define MAYBE_TestSetBounds TestSetBounds
// Disabled as flakey, see http://crbug.com/434532 for details.
#define MAYBE_TestSetSizeConstraints DISABLED_TestSetSizeConstraints
#endif

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestDeprecatedBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testDeprecatedBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestInitialBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testInitialBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestInitialConstraints) {
  ASSERT_TRUE(RunAppWindowAPITest("testInitialConstraints")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestSetBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testSetBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, MAYBE_TestSetSizeConstraints) {
  ASSERT_TRUE(RunAppWindowAPITest("testSetSizeConstraints")) << message_;
}

// Flaky failures on mac_rel and WinXP, see http://crbug.com/324915.
IN_PROC_BROWSER_TEST_F(AppWindowAPITest,
                       DISABLED_TestRestoreGeometryCacheChange) {
  // This test is similar to the other AppWindowAPI tests except that at some
  // point the app will send a 'ListenGeometryChange' message at which point the
  // test will check if the geometry cache entry for the test window has
  // changed. When the change happens, the test will let the app know so it can
  // continue running.
  ExtensionTestMessageListener launched_listener("Launched",
                                                 ReplyBehavior::kWillReply);

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("window_api"));
  EXPECT_TRUE(extension);

  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));

  ExtensionTestMessageListener geometry_listener("ListenGeometryChange",
                                                 ReplyBehavior::kWillReply);

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  launched_listener.Reply("testRestoreAfterGeometryCacheChange");

  ASSERT_TRUE(geometry_listener.WaitUntilSatisfied());

  GeometryCacheChangeHelper geo_change_helper_1(
      AppWindowGeometryCache::Get(browser()->profile()), extension->id(),
      // The next line has information that has to stay in sync with the app.
      "test-ra", gfx::Rect(200, 200, 200, 200));

  GeometryCacheChangeHelper geo_change_helper_2(
      AppWindowGeometryCache::Get(browser()->profile()), extension->id(),
      // The next line has information that has to stay in sync with the app.
      "test-rb", gfx::Rect(200, 200, 200, 200));

  // These calls will block until the app window geometry cache will change.
  geo_change_helper_1.WaitForEntirelyChanged();
  geo_change_helper_2.WaitForEntirelyChanged();

  ResultCatcher catcher;
  geometry_listener.Reply("");
  ASSERT_TRUE(catcher.GetNextResult());
}

// TODO(benwells): Implement on Mac.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestFrameColors) {
  ASSERT_TRUE(RunAppWindowAPITest("testFrameColors")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestVisibleOnAllWorkspaces) {
  ASSERT_TRUE(
      RunAppWindowAPITestAndWaitForRoundTrip("testVisibleOnAllWorkspaces"))
      << message_;
}
