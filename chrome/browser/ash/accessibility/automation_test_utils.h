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

  // Waits for the root web area with the given URL to be loaded.
  // Note that the URL should not use backtick quotes, or if so they
  // should be escaped, to avoid collisions with the Javascript
  // strings.
  void WaitForPageLoad(const std::string& url);

  // Gets the bounds of the root web area with the given URL in
  // density-independent pixels.
  // Note that the URL should not use backtick quotes, or if so they
  // should be escaped, to avoid collisions with the Javascript
  // strings.
  gfx::Rect GetBoundsOfRootWebArea(const std::string& url);

  // Gets the value of the node with the given `class_name`.
  std::string GetValueForNodeWithClassName(const std::string& class_name);

  // Waits for the node with the given `class_name` to have the value `value`.
  void WaitForNodeWithClassNameAndValue(const std::string& class_name,
                                        const std::string& value);

  // Gets the bounds of the automation node with the given
  // `name` and `role` in density-independent pixels. Will wait
  // for the node to exist if it does not exist already.
  gfx::Rect GetNodeBoundsInRoot(const std::string& name,
                                const std::string& role);

  // Gets the bounds of the automation node with the given
  // `class_name` in density-independent pixels. Will wait
  // for the node to exist if it does not exist already.
  gfx::Rect GetBoundsForNodeInRootByClassName(const std::string& class_name);

  // Sets focus on the automation node with the given `name` and `role`.
  // Will wait for the node to exist if it does not exist already.
  void SetFocusOnNode(const std::string& name, const std::string& role);

  // Checks if a given node exists in the tree. Does not wait if the
  // node does not exist.
  bool NodeExistsNoWait(const std::string& name, const std::string& role);

  // Does the default action on the node with `name` and `role`.
  void DoDefault(const std::string& name, const std::string& role);

  // Various event waiters. This is the automation equivalent of
  // AccessibilityNotificationWaiter.

  // Waits for a chrome.automation.EventType.TEXT_SELECTION_CHANGED event to be
  // fired on the desktop node.
  void WaitForTextSelectionChangedEvent();

  // Waits for a chrome.automation.EventType.VALUE_CHANGED event to be fired
  // on the desktop node.
  void WaitForValueChangedEvent();

  // Waits for a chrome.automation.EventType.CHILDREN_CHANGED event to be fired
  // on the desktop node.
  void WaitForChildrenChangedEvent();

  // Waits for there to be `num` tabs in the tabstrip with regex name `name`.
  void WaitForNumTabsWithRegexName(int num, const std::string& name);

 private:
  std::string ExecuteScriptInExtensionPage(const std::string& script);
  std::string extension_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOMATION_TEST_UTILS_H_
