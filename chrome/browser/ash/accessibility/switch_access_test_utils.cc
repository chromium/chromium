// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/switch_access_test_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry_test_helper.h"

namespace {

constexpr char kTestSupportPathMV2[] =
    "chrome/browser/resources/chromeos/accessibility/switch_access/mv2/"
    "test_support.js";
constexpr char kTestSupportPathMV3[] =
    "chrome/browser/resources/chromeos/accessibility/switch_access/mv3/"
    "test_support.js";
}  // namespace

namespace ash {

SwitchAccessTestUtils::SwitchAccessTestUtils(Profile* profile) {
  profile_ = profile;
  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      profile_, extension_misc::kSwitchAccessExtensionId);
}

SwitchAccessTestUtils::~SwitchAccessTestUtils() = default;

void SwitchAccessTestUtils::EnableSwitchAccess(
    const std::set<int>& select_key_codes,
    const std::set<int>& next_key_codes,
    const std::set<int>& previous_key_codes) {
  AccessibilityManager* manager = AccessibilityManager::Get();

  // Watch events from an MV2 extension which runs in a background page.
  extensions::ExtensionHostTestHelper host_helper(
      profile_, extension_misc::kSwitchAccessExtensionId);
  // Watch events from an MV3 extension which runs in a service worker.
  extensions::ExtensionRegistryTestHelper observer(
      extension_misc::kSwitchAccessExtensionId, profile_);

  manager->SetSwitchAccessEnabled(true);

  if (observer.WaitForManifestVersion() == 3) {
    version_ = ManifestVersion::kThree;
    observer.WaitForServiceWorkerStart();
  } else {
    version_ = ManifestVersion::kTwo;
    host_helper.WaitForHostCompletedFirstLoad();
  }

  manager->SetSwitchAccessKeysForTest(
      select_key_codes, prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes);
  manager->SetSwitchAccessKeysForTest(
      next_key_codes, prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes);
  manager->SetSwitchAccessKeysForTest(
      previous_key_codes,
      prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes);

  EXPECT_TRUE(manager->IsSwitchAccessEnabled());

  InjectFocusRingWatcher();
}

void SwitchAccessTestUtils::WaitForFocusRing(const std::string& type,
                                             const std::string& role,
                                             const std::string& name) {
  ASSERT_TRUE(type == "primary" || type == "preview");
  std::string script = base::StringPrintf(
      R"JS(
          waitForFocusRing("%s", "%s", "%s", () => {
            chrome.test.sendScriptResult('ok');
          });
        )JS",
      type.c_str(), role.c_str(), name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::DoDefault(const std::string& name) {
  std::string script = base::StringPrintf(
      R"JS(
        doDefault("%s", () => {
          chrome.test.sendScriptResult('ok');
        });
      )JS",
      name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::PointScanClick(const int x, const int y) {
  std::string script = base::StringPrintf(
      R"JS(
        pointScanClick("%d", "%d", () => {
          chrome.test.sendScriptResult('ok');
        });
      )JS",
      x, y);
  WaitForJS(script);
}

void SwitchAccessTestUtils::WaitForEventOnAutomationNode(
    const std::string& eventType,
    const std::string& name) {
  std::string script = base::StringPrintf(
      R"JS(
          waitForEventOnAutomationNode("%s", "%s", () => {
            chrome.test.sendScriptResult('ok');
          });
        )JS",
      eventType.c_str(), name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::WaitForBackButtonInitialized() {
  std::string script = base::StringPrintf(
      R"JS(
          waitForBackButtonInitialized();
        )JS");
  WaitForJS(script);
}

void SwitchAccessTestUtils::ResetConsoleObserver() {
  console_observer_.reset();
}

void SwitchAccessTestUtils::WaitForJS(const std::string& js_to_eval) {
  base::Value value =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile_, extension_misc::kSwitchAccessExtensionId, js_to_eval,
          extensions::browsertest_util::ScriptUserActivation::kDontActivate);
  ASSERT_EQ(value, "ok");
}

void SwitchAccessTestUtils::InjectFocusRingWatcher() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_support_path = version_ == ManifestVersion::kThree
                               ? source_dir.AppendASCII(kTestSupportPathMV3)
                               : source_dir.AppendASCII(kTestSupportPathMV2);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;

  base::Value value =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile_, extension_misc::kSwitchAccessExtensionId, script);
  ASSERT_EQ("ready", value);
}

}  // namespace ash
