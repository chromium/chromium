// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "ppapi/buildflags/buildflags.h"

namespace content {
class NavigationHandle;
struct WebPluginInfo;
}  // namespace content

class PDFIFrameNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit PDFIFrameNavigationThrottle(content::NavigationHandle* handle);
  ~PDFIFrameNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
#if BUILDFLAG(ENABLE_PLUGINS)
  // Callback to check on the PDF plugin status after loading the plugin list.
  void OnPluginsLoaded(const std::vector<content::WebPluginInfo>& plugins);
#endif

  // Loads the placeholder HTML into the IFRAME.
  void LoadPlaceholderHTML();

  base::WeakPtrFactory<PDFIFrameNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PLUGINS_PDF_IFRAME_NAVIGATION_THROTTLE_H_
