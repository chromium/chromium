// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_TEST_THEME_SERVICE_CHANGED_WAITER_H_
#define CHROME_BROWSER_THEMES_TEST_THEME_SERVICE_CHANGED_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/themes/theme_service_observer.h"

class ThemeService;

namespace test {

// Waits for a call to |OnThemeChanged()| for testing. It starts observing calls
// on construction, and can only be used to wait once.
class ThemeServiceChangedWaiter : public ThemeServiceObserver {
 public:
  explicit ThemeServiceChangedWaiter(ThemeService* service);
  ThemeServiceChangedWaiter(const ThemeServiceChangedWaiter&) = delete;
  ThemeServiceChangedWaiter& operator=(const ThemeServiceChangedWaiter&) =
      delete;
  ~ThemeServiceChangedWaiter() override;

  // ThemeServiceObserver implementation.
  void OnThemeChanged() override;

  // Waits for an |OnThemeChanged()| call from |service_|.
  void WaitForThemeChanged();

 private:
  base::RunLoop run_loop_;

  const raw_ptr<ThemeService> service_;
};

}  // namespace test

#endif  // CHROME_BROWSER_THEMES_TEST_THEME_SERVICE_CHANGED_WAITER_H_
