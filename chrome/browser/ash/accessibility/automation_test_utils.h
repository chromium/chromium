// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOMATION_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOMATION_TEST_UTILS_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace ash {

// Class that provides Ash browsertests support via the Automation API.
class AutomationTestUtils {
 public:
  explicit AutomationTestUtils(const std::string& extension_id);
  ~AutomationTestUtils();
  AutomationTestUtils(const AutomationTestUtils&) = delete;
  AutomationTestUtils& operator=(const AutomationTestUtils&) = delete;

  // Should be called once the extension under test is loaded.
  void SetUpTestSupport();

  // Gets the bounds of the automation node with the given
  // `name` and `role` in density-independent pixels.
  gfx::Rect GetNodeBoundsInRoot(const std::string& name,
                                const std::string& role);

 private:
  std::string ExecuteScriptInExtensionPage(const std::string& script);
  std::string extension_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOMATION_TEST_UTILS_H_
