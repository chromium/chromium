// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#include <memory>

class AutomationManagerLacros;

namespace crosapi {
class TaskManagerLacros;
}

// Browser initialization for Lacros.
class ChromeBrowserMainExtraPartsLacros : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsLacros();
  ChromeBrowserMainExtraPartsLacros(const ChromeBrowserMainExtraPartsLacros&) =
      delete;
  ChromeBrowserMainExtraPartsLacros& operator=(
      const ChromeBrowserMainExtraPartsLacros&) = delete;
  ~ChromeBrowserMainExtraPartsLacros() override;

 private:
  // ChromeBrowserMainExtraParts:
  void PostBrowserStart() override;

  std::unique_ptr<AutomationManagerLacros> automation_manager_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
