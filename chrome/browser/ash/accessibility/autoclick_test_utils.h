// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOCLICK_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOCLICK_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"

class PrefChangeRegistrar;
class Profile;

namespace base {
class RunLoop;
}  // namespace base

namespace ui {
namespace test {
class EventGenerator;
}  // namespace test
}  // namespace ui

namespace ash {
enum class AutoclickEventType;
class AutomationTestUtils;
class ExtensionConsoleErrorObserver;

// A class that can be used to exercise Autoclick in browsertests.
class AutoclickTestUtils {
 public:
  explicit AutoclickTestUtils(Profile* profile);
  ~AutoclickTestUtils();
  AutoclickTestUtils(const AutoclickTestUtils&) = delete;
  AutoclickTestUtils& operator=(const AutoclickTestUtils&) = delete;

  void LoadAutoclick(bool install_automation_utils = true);
  void SetAutoclickDelayMs(int ms);
  void SetAutoclickEventTypeWithHover(ui::test::EventGenerator* generator,
                                      AutoclickEventType type);
  void WaitForPageLoad(const std::string& url);
  void WaitForTextSelectionChangedEvent();
  void HoverOverHtmlElement(ui::test::EventGenerator* generator,
                            const std::string& name,
                            const std::string& role);
  void ObserveFocusRings();
  void WaitForFocusRingChanged();
  // Waits for the given node to exist, then returns its bounds.
  gfx::Rect GetNodeBoundsInRoot(const std::string& name,
                                const std::string& role);
  gfx::Rect GetBoundsForNodeInRootByClassName(const std::string& class_name);

 private:
  void WaitForAutoclickReady();
  void OnEventTypePrefChanged();
  void OnFocusRingChanged();
  base::WeakPtr<AutoclickTestUtils> GetWeakPtr();

  raw_ptr<Profile> profile_;
  std::unique_ptr<AutomationTestUtils> automation_utils_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::OnceClosure pref_change_waiter_;
  std::unique_ptr<base::RunLoop> loop_runner_;
  base::WeakPtrFactory<AutoclickTestUtils> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_AUTOCLICK_TEST_UTILS_H_
