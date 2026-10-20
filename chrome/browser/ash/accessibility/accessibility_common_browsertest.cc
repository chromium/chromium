// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_extension_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry_test_helper.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/gurl.h"

namespace ash {
namespace {

class ShutdownTestExtensionsBrowserClient
    : public extensions::TestExtensionsBrowserClient {
 public:
  explicit ShutdownTestExtensionsBrowserClient(
      content::BrowserContext* main_context)
      : extensions::TestExtensionsBrowserClient(main_context) {}
  ShutdownTestExtensionsBrowserClient(
      const ShutdownTestExtensionsBrowserClient&) = delete;
  ShutdownTestExtensionsBrowserClient& operator=(
      const ShutdownTestExtensionsBrowserClient&) = delete;
  ~ShutdownTestExtensionsBrowserClient() override = default;

  // extensions::TestExtensionsBrowserClient:
  bool IsShuttingDown() override { return is_shutting_down_; }

  void set_is_shutting_down(bool is_shutting_down) {
    is_shutting_down_ = is_shutting_down;
  }

 private:
  bool is_shutting_down_ = false;
};

}  // namespace

class AccessibilityCommonTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  AccessibilityCommonTest() = default;
  ~AccessibilityCommonTest() override = default;
  AccessibilityCommonTest(const AccessibilityCommonTest&) = delete;
  AccessibilityCommonTest& operator=(const AccessibilityCommonTest&) = delete;

  void SetUpOnMainThread() override {
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->GetProfile(),
        extension_misc::kAccessibilityCommonExtensionId);
  }

  void TearDownOnMainThread() override {
    EXPECT_FALSE(console_observer_->HasErrorsOrWarnings())
        << "Found console.warn or console.error with message: "
        << console_observer_->GetErrorOrWarningAt(0);
  }

  bool DoesComponentExtensionExist(const std::string& id) {
    return extensions::ComponentLoader::Get(
               AccessibilityManager::Get()->profile())
        ->Exists(id);
  }

 private:
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

IN_PROC_BROWSER_TEST_P(AccessibilityCommonTest, ToggleFeatures) {
  extensions::ScopedCurrentChannel channel(GetParam());

  AccessibilityManager* manager = AccessibilityManager::Get();
  const auto& enabled_features =
      manager->GetAccessibilityCommonEnabledFeaturesForTest();

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  PrefService* pref_service = manager->profile()->GetPrefs();

  {
    extensions::ExtensionHostTestHelper host_helper(
        manager->profile(), extension_misc::kAccessibilityCommonExtensionId);
    extensions::ExtensionRegistryTestHelper observer(
        extension_misc::kAccessibilityCommonExtensionId, manager->profile());
    pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, true);
    if (observer.WaitForManifestVersion() == 3) {
      observer.WaitForServiceWorkerStart();
    } else {
      host_helper.WaitForHostCompletedFirstLoad();
    }
  }

  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  EXPECT_EQ(2U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_EQ(
      1U, enabled_features.count(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, false);
  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(
      1U, enabled_features.count(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, false);
  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  {
    extensions::ExtensionHostTestHelper host_helper(
        manager->profile(), extension_misc::kChromeVoxExtensionId);
    // Not an accessibility common feature.
    pref_service->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
    host_helper.WaitForHostCompletedFirstLoad();
  }

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));
  EXPECT_TRUE(
      DoesComponentExtensionExist(extension_misc::kChromeVoxExtensionId));
}

INSTANTIATE_TEST_SUITE_P(AllChannels,
                         AccessibilityCommonTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

class AccessibilityCommonFaceGazeTest : public AccessibilityCommonTest {
 public:
  AccessibilityCommonFaceGazeTest() = default;
  ~AccessibilityCommonFaceGazeTest() override = default;
  AccessibilityCommonFaceGazeTest(const AccessibilityCommonFaceGazeTest&) =
      delete;
  AccessibilityCommonFaceGazeTest& operator=(
      const AccessibilityCommonFaceGazeTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(AllChannels,
                         AccessibilityCommonFaceGazeTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

IN_PROC_BROWSER_TEST_P(AccessibilityCommonFaceGazeTest, ToggleFaceGaze) {
  extensions::ScopedCurrentChannel channel(GetParam());

  AccessibilityManager* manager = AccessibilityManager::Get();
  const auto& enabled_features =
      manager->GetAccessibilityCommonEnabledFeaturesForTest();

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  PrefService* pref_service = manager->profile()->GetPrefs();

  // Enable the FaceGaze pref and wait for the accessibility common extension
  // to load.
  extensions::ExtensionHostTestHelper host_helper(
      manager->profile(), extension_misc::kAccessibilityCommonExtensionId);
  extensions::ExtensionRegistryTestHelper observer(
      extension_misc::kAccessibilityCommonExtensionId, manager->profile());
  pref_service->SetBoolean(prefs::kAccessibilityFaceGazeEnabled, true);
  if (observer.WaitForManifestVersion() == 3) {
    observer.WaitForServiceWorkerStart();
  } else {
    host_helper.WaitForHostCompletedFirstLoad();
  }

  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(prefs::kAccessibilityFaceGazeEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  // Disabling the FaceGaze pref should unload the accessibility common
  // extension.
  pref_service->SetBoolean(prefs::kAccessibilityFaceGazeEnabled, false);
  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));
}

using ExtensionConsoleErrorObserverTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionConsoleErrorObserverTest,
                       IgnoresShutdownErrorOnlyDuringShutdown) {
  ExtensionConsoleErrorObserver observer(
      browser()->GetProfile(), extension_misc::kAccessibilityCommonExtensionId);

  auto* original_client = extensions::ExtensionsBrowserClient::Get();
  ShutdownTestExtensionsBrowserClient test_client(browser()->GetProfile());
  extensions::ExtensionsBrowserClient::Set(&test_client);
  base::ScopedClosureRunner reset_client(
      base::BindOnce(&extensions::ExtensionsBrowserClient::Set,
                     base::Unretained(original_client)));

  auto make_runtime_error = [](const std::u16string& message) {
    return std::make_unique<extensions::RuntimeError>(
        extension_misc::kAccessibilityCommonExtensionId,
        /*from_incognito=*/false,
        base::ASCIIToUTF16(
            base::StrCat({"chrome-extension://",
                          extension_misc::kAccessibilityCommonExtensionId,
                          "/background.js"})),
        message, extensions::StackTrace(), GURL(), logging::LOGGING_ERROR,
        /*render_frame_id=*/0, /*render_process_id=*/0);
  };

  auto* error_console = extensions::ErrorConsole::Get(browser()->GetProfile());

  // 1. Non-shutdown error during active test execution -> fails & recorded.
  EXPECT_NONFATAL_FAILURE(
      error_console->ReportError(
          make_runtime_error(u"Uncaught TypeError: Cannot read properties")),
      "Found extension console warning or error with message");
  EXPECT_TRUE(observer.HasErrorsOrWarnings());
  EXPECT_EQ(1u, observer.GetErrorsAndWarningsCount());

  // 2. "The browser is shutting down." during active test execution
  //    (`IsShuttingDown()` returns `false`) -> still fails & recorded.
  EXPECT_NONFATAL_FAILURE(
      error_console->ReportError(make_runtime_error(
          u"Uncaught (in promise) Error: The browser is shutting down.")),
      "Found extension console warning or error with message");
  EXPECT_EQ(2u, observer.GetErrorsAndWarningsCount());

  // 3. Transition to browser shutdown (`IsShuttingDown()` returns `true`):
  //    "The browser is shutting down." -> ignored (no failure, count stays 2).
  test_client.set_is_shutting_down(true);
  error_console->ReportError(make_runtime_error(
      u"Uncaught (in promise) Error: The browser is shutting down."));
  EXPECT_EQ(2u, observer.GetErrorsAndWarningsCount());

  // 4. Any other error during shutdown -> still fails and is recorded.
  EXPECT_NONFATAL_FAILURE(
      error_console->ReportError(
          make_runtime_error(u"Uncaught ReferenceError: foo is not defined")),
      "Found extension console warning or error with message");
  EXPECT_EQ(3u, observer.GetErrorsAndWarningsCount());
}

}  // namespace ash
