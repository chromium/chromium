// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/automation_test_utils.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/browsertest_util.h"

namespace ash {

namespace {

constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/common/"
    "automation_test_support.js";

gfx::Rect StringToRect(const std::string& script_result) {
  std::vector<std::string> tokens = base::SplitString(
      script_result, ",;", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(tokens.size(), 4u);
  int x, y, width, height;
  base::StringToInt(tokens[0], &x);
  base::StringToInt(tokens[1], &y);
  base::StringToInt(tokens[2], &width);
  base::StringToInt(tokens[3], &height);
  return gfx::Rect(x, y, width, height);
}

}  // namespace

AutomationTestUtils::AutomationTestUtils(const std::string& extension_id)
    : extension_id_(extension_id) {}

AutomationTestUtils::~AutomationTestUtils() {}

void AutomationTestUtils::SetUpTestSupport() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;
  ExecuteScriptInExtensionPage(script);
}

void AutomationTestUtils::WaitForPageLoad(const std::string& url) {
  ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.waitForPageLoad(`%s`))JS",
      url.c_str()));
}

gfx::Rect AutomationTestUtils::GetBoundsOfRootWebArea(const std::string& url) {
  std::string script_result = ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.getBoundsForRootWebArea(`%s`))JS",
      url.c_str()));
  return StringToRect(script_result);
}

gfx::Rect AutomationTestUtils::GetNodeBoundsInRoot(const std::string& name,
                                                   const std::string& role) {
  std::string script_result = ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.getBoundsForNode(`%s`, `%s`))JS",
      name.c_str(), role.c_str()));
  return StringToRect(script_result);
}

gfx::Rect AutomationTestUtils::GetBoundsForNodeInRootByClassName(
    const std::string& class_name) {
  std::string script_result = ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.getBoundsForNodeByClassName(`%s`))JS",
      class_name.c_str()));
  return StringToRect(script_result);
}

void AutomationTestUtils::SetFocusOnNode(const std::string& name,
                                         const std::string& role) {
  ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.setFocusOnNode(`%s`, `%s`))JS",
      name.c_str(), role.c_str()));
}

bool AutomationTestUtils::NodeExistsNoWait(const std::string& name,
                                           const std::string& role) {
  std::string script_result = ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.nodeExistsNoWait(`%s`, `%s`))JS",
      name.c_str(), role.c_str()));
  return script_result == "true";
}

void AutomationTestUtils::DoDefault(const std::string& name,
                                    const std::string& role) {
  ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.doDefault(`%s`, `%s`))JS",
      name.c_str(), role.c_str()));
}

void AutomationTestUtils::WaitForTextSelectionChangedEvent() {
  std::string script = R"(
    globalThis.automationTestSupport.waitForTextSelectionChangedEvent();
  )";
  ExecuteScriptInExtensionPage(script);
}

void AutomationTestUtils::WaitForValueChangedEvent() {
  std::string script = R"(
    globalThis.automationTestSupport.waitForValueChangedEvent();
  )";
  ExecuteScriptInExtensionPage(script);
}

void AutomationTestUtils::WaitForChildrenChangedEvent() {
  std::string script = R"(
    globalThis.automationTestSupport.waitForChildrenChangedEvent();
  )";
  ExecuteScriptInExtensionPage(script);
}

void AutomationTestUtils::WaitForNumTabsWithRegexName(int num,
                                                      const std::string& name) {
  std::string script =
      base::StringPrintf(R"(
    globalThis.automationTestSupport.waitForNumTabsWithName(%s, %s);
  )",
                         base::ToString(num).c_str(), name.c_str());
  ExecuteScriptInExtensionPage(script);
}

std::string AutomationTestUtils::GetValueForNodeWithClassName(
    const std::string& class_name) {
  std::string script = base::StringPrintf(R"(
    globalThis.automationTestSupport.getValueForNodeWithClassName(`%s`);
  )",
                                          class_name.c_str());
  return ExecuteScriptInExtensionPage(script);
}

void AutomationTestUtils::WaitForNodeWithClassNameAndValue(
    const std::string& class_name,
    const std::string& value) {
  std::string script = base::StringPrintf(R"(
    globalThis.automationTestSupport.waitForNodeWithClassNameAndValue(
        `%s`, `%s`);)",
                                          class_name.c_str(), value.c_str());
  ExecuteScriptInExtensionPage(script);
}

std::string AutomationTestUtils::ExecuteScriptInExtensionPage(
    const std::string& script) {
  // Note SpokenFeedbackTest uses ExecuteScriptInBackgroundPageDeprecated.
  // It seems that we must use the same method / callback style here for
  // this to run successfully in the ChromeVox extension.
  // TODO(b/290096429): Use non-deprecated method.
  return extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      /*context=*/AccessibilityManager::Get()->profile(),
      /*extension_id=*/extension_id_,
      /*script=*/script);
}

}  // namespace ash
