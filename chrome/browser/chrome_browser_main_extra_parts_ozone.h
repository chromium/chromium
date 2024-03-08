// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsOzone : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsOzone();
  ChromeBrowserMainExtraPartsOzone(const ChromeBrowserMainExtraPartsOzone&) =
      delete;
  ChromeBrowserMainExtraPartsOzone& operator=(
      const ChromeBrowserMainExtraPartsOzone&) = delete;
  ~ChromeBrowserMainExtraPartsOzone() override;

 protected:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateMainMessageLoop() override;
  void PostMainMessageLoopRun() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_OZONE_H_
