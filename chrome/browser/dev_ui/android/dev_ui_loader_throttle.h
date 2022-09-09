// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEV_UI_ANDROID_DEV_UI_LOADER_THROTTLE_H_
#define CHROME_BROWSER_DEV_UI_ANDROID_DEV_UI_LOADER_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace dev_ui {

// For DevUI page navigations, if the DevUI DFM is not installed then delay
// navigation and perform installation. On success, resumes navigation. On
// failure, displays error (retries install on refresh).
class DevUiLoaderThrottle : public content::NavigationThrottle {
 public:
  // Determines whether visiting |url| should trigger DevUI DFM install.
  static bool ShouldInstallDevUiDfm(const GURL& url);

  // Creates a throttle if the DevUI DFM needs to be installed. If the DevUI DFM
  // will be used, is installed, but is not loaded, then resource load takes
  // place as a side effect.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit DevUiLoaderThrottle(content::NavigationHandle* navigation_handle);
  ~DevUiLoaderThrottle() override;
  DevUiLoaderThrottle(const DevUiLoaderThrottle&) = delete;
  const DevUiLoaderThrottle& operator=(const DevUiLoaderThrottle&) = delete;

  // content::NavigationThrottle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;

 private:
  // Callback for dev_ui::DevUiModuleProvider::InstallModule().
  void OnDevUiDfmInstallWithStatus(bool success);

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<DevUiLoaderThrottle> weak_ptr_factory_{this};
};

}  // namespace dev_ui

#endif  // CHROME_BROWSER_DEV_UI_ANDROID_DEV_UI_LOADER_THROTTLE_H_
