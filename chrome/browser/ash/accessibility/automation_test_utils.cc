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

}
AutomationTestUtils::AutomationTestUtils(const std::string& extension_id)
    : extension_id_(extension_id) {}

AutomationTestUtils::~AutomationTestUtils() {}

void AutomationTestUtils::SetUpTestSupport() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;
  ExecuteScriptInExtensionPage(script);
}

gfx::Rect AutomationTestUtils::GetNodeBoundsInRoot(const std::string& name,
                                                   const std::string& role) {
  std::string script_result = ExecuteScriptInExtensionPage(base::StringPrintf(
      R"JS(globalThis.automationTestSupport.getBoundsForNode("%s", "%s"))JS",
      name.c_str(), role.c_str()));
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
