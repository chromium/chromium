// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

// Temporarily used by both Ozone and non-Ozone/X11. Once Ozone becomes default
// on Linux, this class will be used purely by Ozone.
class ChromeBrowserMainExtraPartsOzone : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsOzone();
  ChromeBrowserMainExtraPartsOzone(const ChromeBrowserMainExtraPartsOzone&) =
      delete;
  ChromeBrowserMainExtraPartsOzone& operator=(
      const ChromeBrowserMainExtraPartsOzone&) = delete;
  ~ChromeBrowserMainExtraPartsOzone() override;

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PreEarlyInitialization() override;
  void PostCreateMainMessageLoop() override;
  void PostMainMessageLoopRun() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_
