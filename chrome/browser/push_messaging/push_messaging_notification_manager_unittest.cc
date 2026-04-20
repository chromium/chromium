// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

class PushMessagingNotificationManagerTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(PushMessagingNotificationManagerTest, IsTabVisible) {
  PushMessagingNotificationManager manager(profile());
  GURL origin("https://google.com/");
  GURL origin_with_path = origin.Resolve("/path/");
  NavigateAndCommit(origin_with_path);

  EXPECT_FALSE(manager.IsTabVisible(profile(), nullptr, origin));
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(),
                                    GURL("https://chrome.com/")));
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasHidden();
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));
}

TEST_F(PushMessagingNotificationManagerTest, IsTabVisibleViewSource) {
  PushMessagingNotificationManager manager(profile());

  GURL origin("https://google.com/");
  GURL view_source_page("view-source:https://google.com/path/");

  NavigateAndCommit(view_source_page);

  ASSERT_EQ(view_source_page, web_contents()->GetVisibleURL());
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasHidden();
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(), origin));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
namespace extensions {

using ContextType = extensions::browser_test_util::ContextType;

class ExtensionsPushMessagingNotificationManagerTest
    : public ExtensionServiceTestWithInstall,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionsPushMessagingNotificationManagerTest() = default;

  ExtensionsPushMessagingNotificationManagerTest(
      const ExtensionsPushMessagingNotificationManagerTest&) = delete;
  ExtensionsPushMessagingNotificationManagerTest& operator=(
      const ExtensionsPushMessagingNotificationManagerTest&) = delete;

  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
  }

 private:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that when receiving a push message only service worker-based extensions
// are allowed to bypass/skip the user visible notification requirement only if
// they set `userVisibleOnly` false when they subscribed to the push server.
TEST_P(ExtensionsPushMessagingNotificationManagerTest,
       SkipEnforceUserVisibleOnlyRequirements_ForExtensions) {
  static constexpr char kManifestPersistentBackgroundScript[] =
      R"({"scripts": ["background.js"], "persistent": true})";
  static constexpr char kManifestEventPageBackgroundScript[] =
      R"({"persistent": false,
          "scripts": ["background.js"]
         }
      )";
  static constexpr char kManifestServiceWorkerBackgroundScript[] =
      R"({"service_worker": "background.js"})";

  // Load an extension of ContextType.
  TestExtensionDir test_dir;
  constexpr char kManifest[] =
      R"({
         "name": "Test Extension",
         "manifest_version": %s,
         "version": "0.1",
         "background": %s,
         "permissions": ["notifications"]
       })";
  ContextType extension_context_type = GetParam();
  bool worker_extension = extension_context_type == ContextType::kServiceWorker;
  const char* background_script;
  if (worker_extension) {
    background_script = kManifestServiceWorkerBackgroundScript;
  } else if (extension_context_type == ContextType::kEventPage) {
    background_script = kManifestEventPageBackgroundScript;
  } else {
    background_script = kManifestPersistentBackgroundScript;
  }
  const char* manifest_version = worker_extension ? "3" : "2";
  std::string manifest =
      base::StringPrintf(kManifest, manifest_version, background_script);
  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  bool user_visible_requirement_bypassed = false;
  PushMessagingNotificationManager manager(profile());

  // Inject test clock to control time and silent push budget awards.
  auto clock = std::make_unique<base::SimpleTestClock>();
  clock->SetNow(base::Time::Now());
  base::SimpleTestClock* clock_ptr = clock.get();
  manager.SetBudgetClockForTesting(std::move(clock));

  GURL extension_url = Extension::GetBaseURLFromExtensionId(extension->id());

  // Attempt to bypass while `userVisibleOnly` false.
  {
    // Set engagement score to 100 to allow silent push budget to be awarded.
    site_engagement::SiteEngagementService::Get(profile())
        ->ResetBaseScoreForURL(extension_url, /*score=*/100);
    // Advance time by 1 hour to cause budget to be awarded.
    clock_ptr->Advance(base::Hours(1));

    user_visible_requirement_bypassed = false;
    base::RunLoop wait_for_bypass;
    manager.EnforceUserVisibleOnlyRequirements(
        Extension::GetBaseURLFromExtensionId(extension->id()), 0l,
        // Callback that is called when user visible requirements are bypassed.
        base::BindOnce(
            [](bool* user_visible_requirement_bypassed,
               base::OnceClosure quit_closure,
               bool did_show_generic_notification) {
              *user_visible_requirement_bypassed = true;
              // Generic notifications are shown if the push subscriber didn't
              // show a user visible notification when receiving a push
              // notification.
              EXPECT_FALSE(did_show_generic_notification);
              std::move(quit_closure).Run();
            },
            &user_visible_requirement_bypassed, wait_for_bypass.QuitClosure()),
        /*user_visible_only_bypass=*/true);  // `userVisibleOnly` false

    // Only worker extensions are allowed to bypass the user visible push
    // notification requirement.
    if (worker_extension) {
      EXPECT_TRUE(user_visible_requirement_bypassed);
    } else {
      EXPECT_FALSE(user_visible_requirement_bypassed);
    }

    {
      SCOPED_TRACE(
          "Waiting for EnforceUserVisibleOnlyRequirements with "
          "userVisibleOnly: "
          "false");
      wait_for_bypass.Run();
    }
    EXPECT_TRUE(user_visible_requirement_bypassed);
  }

  // Attempt to bypass while `userVisibleOnly` true.
  {
    // A generic notification is only shown when the push receiver doesn't show
    // a notification and is out of "budget" for silent pushes. We want to
    // confirm that when this budget is exhausted a generic notification is
    // shown. To do this we set the engagement score to 0, and advance the
    // test clock far into the future to expire all existing budget.
    site_engagement::SiteEngagementService::Get(profile())
        ->ResetBaseScoreForURL(extension_url, /*score=*/0);
    clock_ptr->Advance(base::Days(5));

    user_visible_requirement_bypassed = false;
    bool generic_notification_shown = false;
    base::RunLoop wait_for_enforcement;

    manager.EnforceUserVisibleOnlyRequirements(
        Extension::GetBaseURLFromExtensionId(extension->id()), 0l,
        base::BindOnce(
            [](bool* user_visible_requirement_bypassed,
               bool* generic_notification_shown, base::OnceClosure quit_closure,
               bool did_show_generic_notification) {
              *user_visible_requirement_bypassed = true;
              *generic_notification_shown = did_show_generic_notification;
              std::move(quit_closure).Run();
            },
            &user_visible_requirement_bypassed, &generic_notification_shown,
            wait_for_enforcement.QuitClosure()),
        /*user_visible_only_bypass=*/false);  // `userVisibleOnly` true

    // If `userVisibleOnly` true then no matter the extension context type we
    // should not bypass synchronously.
    EXPECT_FALSE(user_visible_requirement_bypassed);

    {
      SCOPED_TRACE(
          "Waiting for EnforceUserVisibleOnlyRequirements with "
          "userVisibleOnly: true");
      wait_for_enforcement.Run();
    }
    EXPECT_TRUE(user_visible_requirement_bypassed);
    EXPECT_TRUE(generic_notification_shown);
  }
}

// Android only supports manifest V3 with service worker.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(
    NonWorkerExtension,
    ExtensionsPushMessagingNotificationManagerTest,
    ::testing::ValuesIn({ContextType::kEventPage,
                         ContextType::kPersistentBackground}));
#endif
INSTANTIATE_TEST_SUITE_P(WorkerBasedExtension,
                         ExtensionsPushMessagingNotificationManagerTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace extensions

#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
