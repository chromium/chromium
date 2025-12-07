// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

class PDFIFrameNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit PDFIFrameNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~PDFIFrameNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // Loads the placeholder HTML into the IFRAME.
  void LoadPlaceholderHTML();
};

#endif  // CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_
