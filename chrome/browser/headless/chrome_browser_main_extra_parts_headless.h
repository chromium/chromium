// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_CHROME_BROWSER_MAIN_EXTRA_PARTS_HEADLESS_H_
#define CHROME_BROWSER_HEADLESS_CHROME_BROWSER_MAIN_EXTRA_PARTS_HEADLESS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace headless {

// This class is used to run headless related code at specific points in the
// chrome browser process life cycle.
class ChromeBrowserMainExtraPartsHeadless : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsHeadless();

  ChromeBrowserMainExtraPartsHeadless(
      const ChromeBrowserMainExtraPartsHeadless&) = delete;
  ChromeBrowserMainExtraPartsHeadless& operator=(
      const ChromeBrowserMainExtraPartsHeadless&) = delete;

  ~ChromeBrowserMainExtraPartsHeadless() override;

  // ChromeBrowserMainExtraParts:
  void PreBrowserStart() override;
};

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_CHROME_BROWSER_MAIN_EXTRA_PARTS_HEADLESS_H_
