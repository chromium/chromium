// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {

using ContextType = ExtensionBrowserTest::ContextType;

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
};

// Tests that when receiving a push message only service worker-based extensions
// are allowed to bypass/skip the user visible notification requirement only if
// they set userVisible == false when they subscribed to the push server.
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

  // Attempt to skip while userVisible == false.
  bool skip_allowed = false;
  PushMessagingNotificationManager manager(profile());
  manager.EnforceUserVisibleOnlyRequirements(
      Extension::GetBaseURLFromExtensionId(extension->id()), 0l,
      // Callback that is called when user visible requirements are skipped.
      base::BindOnce(
          [](bool* skip_allowed, bool did_show_generic_notification) {
            *skip_allowed = true;
            // Generic notifications are shown if the push subscriber didn't
            // show a user visible notification when receiving a push
            // notification.
            EXPECT_FALSE(did_show_generic_notification);
          },
          &skip_allowed),
      /*requested_user_visible_only=*/true);  // userVisible == false

  // Only workers are allowed to skip the userVisible notification.
  if (worker_extension) {
    EXPECT_TRUE(skip_allowed);
  } else {
    EXPECT_FALSE(skip_allowed);
  }

  // Attempt to skip while userVisible == true.
  skip_allowed = false;
  manager.EnforceUserVisibleOnlyRequirements(
      Extension::GetBaseURLFromExtensionId(extension->id()), 0l,
      // Callback that is called when user visible requirements are skipped.
      base::BindOnce(
          [](bool* skip_allowed, bool did_show_generic_notification) {
            *skip_allowed = true;
            // Generic notifications are shown if the push subscriber didn't
            // show a user visible notification when receiving a push
            // notification.
            EXPECT_FALSE(did_show_generic_notification);
          },
          &skip_allowed),
      /*requested_user_visible_only=*/false);  // userVisible == true

  // If userVisible == true then no matter the extension context type we should
  // not skip.
  EXPECT_FALSE(skip_allowed);
}

INSTANTIATE_TEST_SUITE_P(
    NonWorkerExtension,
    ExtensionsPushMessagingNotificationManagerTest,
    ::testing::ValuesIn({ContextType::kEventPage,
                         ContextType::kPersistentBackground}));
INSTANTIATE_TEST_SUITE_P(WorkerBasedExtension,
                         ExtensionsPushMessagingNotificationManagerTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace extensions

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
