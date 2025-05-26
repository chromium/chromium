// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::WaitKioskLaunched;

namespace {

// The key used to store `kTestValue` in `chrome.storage` APIs.
constexpr std::string_view kTestKey = "test_key";

// The value to be stored in `chrome.storage` APIs.
constexpr std::string_view kTestValue = "stored_value";

bool SetInExtensionStorage(Profile& profile,
                           const extensions::ExtensionId& id,
                           std::string_view value) {
  constexpr std::string_view kOk = "ok";
  std::string script = base::StringPrintf(R"(
      chrome.storage.local.set(
        {%s: '%s'},
        () => chrome.test.sendScriptResult('%s'));)",
                                          kTestKey, value, kOk);
  auto result = extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      &profile, extension_misc::kChromeVoxExtensionId, script);
  return result == base::Value(kOk);
}

std::string GetFromExtensionStorage(Profile& profile,
                                    const extensions::ExtensionId& id) {
  std::string script = base::StringPrintf(R"(
      chrome.storage.local.get(
        '%s',
        (result) => {
          const value = result.%s;
          chrome.test.sendScriptResult(value == undefined ? "<none>" : value);
        });)",
                                          kTestKey, kTestKey);
  auto result = extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      &profile, extension_misc::kChromeVoxExtensionId, script);
  CHECK(result.is_string()) << "Script result: " << result.DebugString();
  return result.GetString();
}

// Helper class to wait until an extension is ready.
class ExtensionReadyWaiter : public extensions::ExtensionRegistryObserver {
 public:
  ExtensionReadyWaiter(Profile& profile,
                       const extensions::ExtensionId& extension_id)
      : extension_id_(extension_id) {
    auto& registry = CHECK_DEREF(extensions::ExtensionRegistry::Get(&profile));
    if (registry.ready_extensions().Contains(extension_id)) {
      ready_future_.SetValue();
    } else {
      observation_.Observe(&registry);
    }
  }

  [[nodiscard]] bool Wait() { return ready_future_.Wait(); }

 private:
  // extensions::ExtensionRegistryObserver:
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override {
    if (extension->id() == extension_id_) {
      ready_future_.SetValue();
      observation_.Reset();
    }
  }

  // The extension ID to wait for.
  const extensions::ExtensionId extension_id_;

  // The future to be set once the extension is ready.
  base::test::TestFuture<void> ready_future_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      observation_{this};
};

bool EnableChromeVoxExtension(Profile& profile) {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  extensions::ExtensionHostTestHelper helper(
      &profile, extension_misc::kChromeVoxExtensionId);
  return ExtensionReadyWaiter(profile, extension_misc::kChromeVoxExtensionId)
             .Wait() &&
         helper.WaitForHostCompletedFirstLoad() != nullptr;
}

bool DisableChromeVoxExtension(Profile& profile) {
  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  auto& registry = CHECK_DEREF(extensions::ExtensionRegistry::Get(&profile));
  return base::test::RunUntil([&registry] {
    return !registry.ready_extensions().Contains(
        extension_misc::kChromeVoxExtensionId);
  });
}

}  // namespace

// Verifies behavior of accessibility extensions in Kiosk.
class KioskAccessibilityExtensionTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskAccessibilityExtensionTest() = default;
  KioskAccessibilityExtensionTest(const KioskAccessibilityExtensionTest&) =
      delete;
  KioskAccessibilityExtensionTest& operator=(
      const KioskAccessibilityExtensionTest&) = delete;
  ~KioskAccessibilityExtensionTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

 private:
  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/GetParam(),
  };
};

IN_PROC_BROWSER_TEST_P(KioskAccessibilityExtensionTest,
                       KeepsStateOnExtensionRestart) {
  auto& profile = CurrentProfile();

  ASSERT_TRUE(EnableChromeVoxExtension(profile));

  // Set `kTestValue` in storage and verify it is saved.
  ASSERT_TRUE(SetInExtensionStorage(
      profile, extension_misc::kChromeVoxExtensionId, kTestValue));
  EXPECT_EQ(kTestValue, GetFromExtensionStorage(
                            profile, extension_misc::kChromeVoxExtensionId));

  // Disable and re-enable the extension to simulate a restart in-session.
  ASSERT_TRUE(DisableChromeVoxExtension(profile));
  ASSERT_TRUE(EnableChromeVoxExtension(profile));

  // Verify the saved data is still there.
  EXPECT_EQ(kTestValue, GetFromExtensionStorage(
                            profile, extension_misc::kChromeVoxExtensionId));
}

// This test verifies that accessibility extensions do not preserve any local
// data in-between session, as opposed to what they usually do in user sessions.
// See crbug.com/1049566
IN_PROC_BROWSER_TEST_P(KioskAccessibilityExtensionTest,
                       PRE_ClearsStateOnSessionRestart) {
  auto& profile = CurrentProfile();

  ASSERT_TRUE(EnableChromeVoxExtension(profile));

  // Set `kTestValue` in storage and verify it is saved in the current session.
  ASSERT_TRUE(SetInExtensionStorage(
      profile, extension_misc::kChromeVoxExtensionId, kTestValue));
  EXPECT_EQ(kTestValue, GetFromExtensionStorage(
                            profile, extension_misc::kChromeVoxExtensionId));
}

IN_PROC_BROWSER_TEST_P(KioskAccessibilityExtensionTest,
                       ClearsStateOnSessionRestart) {
  auto& profile = CurrentProfile();

  ASSERT_TRUE(EnableChromeVoxExtension(profile));

  // The value set in the PRE test is cleared cleared after a session restart.
  EXPECT_EQ("<none>", GetFromExtensionStorage(
                          profile, extension_misc::kChromeVoxExtensionId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAccessibilityExtensionTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
