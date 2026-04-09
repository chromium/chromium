// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FOR_TESTING_CHROME_BROWSER_MAIN_EXTRA_PARTS_CFT_H_
#define CHROME_BROWSER_CHROME_FOR_TESTING_CHROME_BROWSER_MAIN_EXTRA_PARTS_CFT_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace chrome_for_testing {

// This class is used to run Chrome for Testing  related code at specific points
// in the Chrome browser process life cycle.
class ChromeBrowserMainExtraPartsCft : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsCft();

  ChromeBrowserMainExtraPartsCft(const ChromeBrowserMainExtraPartsCft&) =
      delete;
  ChromeBrowserMainExtraPartsCft& operator=(
      const ChromeBrowserMainExtraPartsCft&) = delete;

  ~ChromeBrowserMainExtraPartsCft() override;

  // ChromeBrowserMainExtraParts:
  void PreBrowserStart() override;
};

}  // namespace chrome_for_testing

#endif  // CHROME_BROWSER_CHROME_FOR_TESTING_CHROME_BROWSER_MAIN_EXTRA_PARTS_CFT_H_
