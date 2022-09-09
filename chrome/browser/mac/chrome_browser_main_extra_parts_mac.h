// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_CHROME_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
#define CHROME_BROWSER_MAC_CHROME_BROWSER_MAIN_EXTRA_PARTS_MAC_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace display {
class ScopedNativeScreen;
}

class ChromeBrowserMainExtraPartsMac : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsMac();
  ChromeBrowserMainExtraPartsMac(const ChromeBrowserMainExtraPartsMac&) =
      delete;
  ChromeBrowserMainExtraPartsMac& operator=(
      const ChromeBrowserMainExtraPartsMac&) = delete;
  ~ChromeBrowserMainExtraPartsMac() override;

  // ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override;

 private:
  std::unique_ptr<display::ScopedNativeScreen> screen_;
};

#endif  // CHROME_BROWSER_MAC_CHROME_BROWSER_MAIN_EXTRA_PARTS_MAC_H_
