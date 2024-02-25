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
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"

namespace {
constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/switch_access/"
    "test_support.js";
}  // namespace

namespace ash {

SwitchAccessTestUtils::SwitchAccessTestUtils(Profile* profile) {
  profile_ = profile;
  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      profile_, extension_misc::kSwitchAccessExtensionId);
}

SwitchAccessTestUtils::~SwitchAccessTestUtils() {}

void SwitchAccessTestUtils::EnableSwitchAccess(
    const std::set<int>& select_key_codes,
    const std::set<int>& next_key_codes,
    const std::set<int>& previous_key_codes) {
  AccessibilityManager* manager = AccessibilityManager::Get();

  extensions::ExtensionHostTestHelper host_helper(
      profile_, extension_misc::kSwitchAccessExtensionId);
  manager->SetSwitchAccessEnabled(true);
  host_helper.WaitForHostCompletedFirstLoad();

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
            window.domAutomationController.send('ok');
          });
        )JS",
      type.c_str(), role.c_str(), name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::DoDefault(const std::string& name) {
  std::string script = base::StringPrintf(
      R"JS(
        doDefault("%s", () => {
          window.domAutomationController.send('ok');
        });
      )JS",
      name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::PointScanClick(const int x, const int y) {
  std::string script = base::StringPrintf(
      R"JS(
        pointScanClick("%d", "%d", () => {
          window.domAutomationController.send('ok');
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
            window.domAutomationController.send('ok');
          });
        )JS",
      eventType.c_str(), name.c_str());
  WaitForJS(script);
}

void SwitchAccessTestUtils::WaitForJS(const std::string& js_to_eval) {
  std::string result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
          profile_, extension_misc::kSwitchAccessExtensionId, js_to_eval,
          extensions::browsertest_util::ScriptUserActivation::kDontActivate);
  ASSERT_EQ(result, "ok");
}

void SwitchAccessTestUtils::InjectFocusRingWatcher() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;

  std::string result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
          profile_, extension_misc::kSwitchAccessExtensionId, script);
  ASSERT_EQ("ready", result);
}

}  // namespace ash
