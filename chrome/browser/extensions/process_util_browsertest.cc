// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_util.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using process_util::GetPersistentBackgroundPageState;
using process_util::PersistentBackgroundPageState;

class ProcessUtilBrowserTest : public ExtensionBrowserTest {
 public:
  ProcessUtilBrowserTest() = default;
  ~ProcessUtilBrowserTest() override = default;

  const Extension* EnableInIncognitoAndWaitForBackgroundPage(
      const Extension& extension) {
    // Cache the ID, since we'll be invalidated the extension.
    const ExtensionId extension_id = extension.id();
    ExtensionHostTestHelper background_ready(profile(), extension_id);
    background_ready.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    // Enable the extension in incognito, and wait for it to reload (including
    // the background page being ready).
    util::SetIsIncognitoEnabled(extension.id(), profile(), true);
    background_ready.WaitForDocumentElementAvailable();
    // Get the reloaded version of the extension.
    return extension_registry()->enabled_extensions().GetByID(extension_id);
  }

  const Extension* LoadExtensionAndWaitForBackgroundPage(
      const base::FilePath& file_path) {
    // LoadExtension() automatically waits for the background page to load.
    const Extension* extension = LoadExtension(file_path);
    return extension;
  }
};

// Tests GetPersistentBackgroundPageState() with a spanning-mode
// extension (which is the default extension behavior).
IN_PROC_BROWSER_TEST_F(ProcessUtilBrowserTest,
                       BackgroundPageLoading_SpanningMode) {
  constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2,
           "background": {
             "persistent": true,
             "scripts": ["background.js"]
           }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Empty");

  const Extension* extension =
      LoadExtensionAndWaitForBackgroundPage(test_dir.UnpackedPath());
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension));

  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, profile()));

  extension = EnableInIncognitoAndWaitForBackgroundPage(*extension);

  // NOTE: We deliberately use chrome::OpenURLOffTheRecord() here (instead of
  // InProcessBrowserTest::OpenURLOffTheRecord() or CreateIncognitoBrowser())
  // because we need the process of opening to be asynchronous for the next
  // assertion.
  chrome::OpenURLOffTheRecord(profile(), GURL("about:blank"));

  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  ASSERT_TRUE(incognito_profile);

  // NOTE: These are actually the same background page (since it runs in
  // spanning mode), but we check the result for both profiles. Since it refers
  // to the same page, even though the incognito browser isn't fully ready, the
  // extension has already loaded.
  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, profile()));
  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, incognito_profile));
}

// Tests GetPersistentBackgroundPageState() with a split-mode
// extension.
IN_PROC_BROWSER_TEST_F(ProcessUtilBrowserTest,
                       BackgroundPageLoading_SplitMode) {
  constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2,
           "background": {
             "persistent": true,
             "scripts": ["background.js"]
           },
           "incognito": "split"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Empty");

  const Extension* extension =
      LoadExtensionAndWaitForBackgroundPage(test_dir.UnpackedPath());
  EXPECT_TRUE(IncognitoInfo::IsSplitMode(extension));

  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, profile()));

  extension = EnableInIncognitoAndWaitForBackgroundPage(*extension);

  // NOTE: We deliberately use chrome::OpenURLOffTheRecord() here (instead of
  // InProcessBrowserTest::OpenURLOffTheRecord() or CreateIncognitoBrowser())
  // because we need the process of opening to be asynchronous for the next
  // assertion.
  chrome::OpenURLOffTheRecord(profile(), GURL("about:blank"));

  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  ASSERT_TRUE(incognito_profile);

  // The on-the-record page should be ready, but not the incognito version
  // (since it should still be loading).
  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, profile()));
  EXPECT_EQ(PersistentBackgroundPageState::kNotReady,
            GetPersistentBackgroundPageState(*extension, incognito_profile));

  // Wait for the incognito profile to finish.
  ExtensionBackgroundPageWaiter(incognito_profile, *extension)
      .WaitForBackgroundOpen();
  // Now, both the incognito and on-the-record pages should should be ready.
  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, profile()));
  EXPECT_EQ(PersistentBackgroundPageState::kReady,
            GetPersistentBackgroundPageState(*extension, incognito_profile));
}

// Tests that GetPersistentBackgroundPageState() returns kInvalid for
// non-persitent extension types.
IN_PROC_BROWSER_TEST_F(ProcessUtilBrowserTest,
                       BackgroundPageLoading_NonPersistent) {
  constexpr char kEventPageManifest[] =
      R"({
           "name": "Test Event Page",
           "version": "0.1",
           "manifest_version": 2,
           "background": {
             "persistent": false,
             "scripts": ["background.js"]
           }
         })";
  TestExtensionDir test_event_page_dir;
  test_event_page_dir.WriteManifest(kEventPageManifest);
  test_event_page_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                                "chrome.test.sendMessage('Event Page Ready');");

  // Load the extension and wait for the background context to spin up. Even
  // though the background has loaded, since it's not a persistent background
  // page, GetPersistentBackgroundPageState() should return kInvalid.
  ExtensionTestMessageListener event_page_listener("Event Page Ready");
  const Extension* event_page =
      LoadExtension(test_event_page_dir.UnpackedPath());
  ASSERT_TRUE(event_page);
  ASSERT_TRUE(event_page_listener.WaitUntilSatisfied());
  EXPECT_EQ(PersistentBackgroundPageState::kInvalid,
            GetPersistentBackgroundPageState(*event_page, profile()));

  // Repeat the test with a SW based extension.
  constexpr char kServiceWorkerManifest[] =
      R"({
           "name": "Test Service Worker",
           "version": "0.1",
           "manifest_version": 3,
           "background": {
             "service_worker": "background.js"
           }
         })";
  TestExtensionDir test_service_worker_dir;
  test_service_worker_dir.WriteManifest(kServiceWorkerManifest);
  test_service_worker_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      "chrome.test.sendMessage('Service Worker Ready');");

  ExtensionTestMessageListener service_worker_listener("Service Worker Ready");
  const Extension* service_worker =
      LoadExtension(test_service_worker_dir.UnpackedPath());
  ASSERT_TRUE(service_worker);
  ASSERT_TRUE(service_worker_listener.WaitUntilSatisfied());
  EXPECT_EQ(PersistentBackgroundPageState::kInvalid,
            GetPersistentBackgroundPageState(*event_page, profile()));
}

}  // namespace extensions
