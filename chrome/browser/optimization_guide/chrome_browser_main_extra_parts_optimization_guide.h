// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_BROWSER_MAIN_EXTRA_PARTS_OPTIMIZATION_GUIDE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_BROWSER_MAIN_EXTRA_PARTS_OPTIMIZATION_GUIDE_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

// This class is used to initialize the optimization guide model store which is
// install-wide, as part of Chrome browser process startup.
class ChromeBrowserMainExtraPartsOptimizationGuide
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsOptimizationGuide() = default;

  // ChromeBrowserMainExtraParts implementation:
  void PreCreateThreads() override;
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_BROWSER_MAIN_EXTRA_PARTS_OPTIMIZATION_GUIDE_H_
