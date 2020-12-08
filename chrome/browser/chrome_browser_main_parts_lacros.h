// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_LACROS_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_LACROS_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_linux.h"

class MetricsReportingObserver;

// Startup and shutdown code for Lacros. See ChromeBrowserMainParts for details.
class ChromeBrowserMainPartsLacros : public ChromeBrowserMainPartsLinux {
 public:
  ChromeBrowserMainPartsLacros(const content::MainFunctionParams& parameters,
                               StartupData* startup_data);
  ChromeBrowserMainPartsLacros(const ChromeBrowserMainPartsLacros&) = delete;
  ChromeBrowserMainPartsLacros& operator=(const ChromeBrowserMainPartsLacros&) =
      delete;
  ~ChromeBrowserMainPartsLacros() override;

  // ChromeBrowserMainParts:
  int PreEarlyInitialization() override;

 private:
  std::unique_ptr<MetricsReportingObserver> metrics_reporting_observer_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_PARTS_LACROS_H_
