// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_

#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"

class ChromeBrowserMainExtraPartsLinux
    : public ChromeBrowserMainExtraPartsOzone {
 public:
  ChromeBrowserMainExtraPartsLinux();
  ChromeBrowserMainExtraPartsLinux(const ChromeBrowserMainExtraPartsLinux&) =
      delete;
  ChromeBrowserMainExtraPartsLinux& operator=(
      const ChromeBrowserMainExtraPartsLinux&) = delete;
  ~ChromeBrowserMainExtraPartsLinux() override;

  static void InitOzonePlatformHint();

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostBrowserStart() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
