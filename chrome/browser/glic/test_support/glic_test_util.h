// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace glic {

namespace prefs {
enum class FreStatus;
}  // namespace prefs

// Provides deterministic browser activation behavior.
// Useful in browser tests where focus is not reliable.
class BrowserActivator : public BrowserListObserver {
 public:
  // The different modes in which browser activation can be controlled.
  enum class Mode {
    // Support a single browser, crash if more than one browser is created at
    // one time. Activates the browser when it is created. This is the default
    // mode, to notify test authors that special consideration is necessary.
    kSingleBrowser,
    // Always keep the first browser active.
    kFirst,
    // Use SetActive() to set the active browser.
    kManual,
  };

  BrowserActivator();
  ~BrowserActivator() override;

  // Sets the browser activation mode.
  void SetMode(Mode mode);

  // Sets the active browser. Switches to `Mode::kManual`.
  void SetActive(Browser* browser);

  // BrowserListObserver impl.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  void SetActivePrivate(Browser* browser);

  Mode mode_ = Mode::kSingleBrowser;
  base::WeakPtr<Browser> active_browser_;
  std::unique_ptr<views::Widget::PaintAsActiveLock> active_lock_;
};

// Signs in a primary account, accepts the FRE, and enables model execution
// capability for that profile. browser_tests and interactive_ui_tests should
// use GlicTestEnvironment. These methods are for unit_tests.
void ForceSigninAndModelExecutionCapability(Profile* profile);
void SigninWithPrimaryAccount(Profile* profile);
void SetModelExecutionCapability(Profile* profile, bool enabled);
void SetFRECompletion(Profile* profile, prefs::FreStatus fre_status);

void InvalidateAccount(Profile* profile);
void ReauthAccount(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_UTIL_H_
