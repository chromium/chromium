// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SWITCH_ACCESS_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SWITCH_ACCESS_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

namespace ash {
class ExtensionConsoleErrorObserver;

// A class that can be used to exercise Switch Access in browsertests.
class SwitchAccessTestUtils {
 public:
  explicit SwitchAccessTestUtils(Profile* profile);
  ~SwitchAccessTestUtils();
  SwitchAccessTestUtils(const SwitchAccessTestUtils&) = delete;
  SwitchAccessTestUtils& operator=(const SwitchAccessTestUtils&) = delete;

  void EnableSwitchAccess(const std::set<int>& select_key_codes,
                          const std::set<int>& next_key_codes,
                          const std::set<int>& previous_key_codes);
  // Type represents the type of focus ring, either 'primary' or 'preview'.
  // Role and Name represent the automation role and name of the target node.
  void WaitForFocusRing(const std::string& type,
                        const std::string& role,
                        const std::string& name);
  void DoDefault(const std::string& name);
  // X and Y are assumed to be coordinates within the screen.
  void PointScanClick(const int x, const int y);
  // EventType should be an automation EventType.
  // Name represents the name of the target node.
  void WaitForEventOnAutomationNode(const std::string& eventType,
                                    const std::string& name);

 private:
  void WaitForJS(const std::string& js_to_eval);
  void InjectFocusRingWatcher();

  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SWITCH_ACCESS_TEST_UTILS_H_
