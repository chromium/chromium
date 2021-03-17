// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_

#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"

namespace optimization_guide {
namespace android {

// Implementation of OptimizationGuideTabUrlProvider that gets URLs from Android
// browser windows.
class OptimizationGuideTabUrlProviderAndroid
    : public OptimizationGuideTabUrlProvider {
 public:
  explicit OptimizationGuideTabUrlProviderAndroid(Profile* profile);
  ~OptimizationGuideTabUrlProviderAndroid() override;

 private:
  // OptimizationGuideTabUrlProvider:
  const std::vector<content::WebContents*> GetAllWebContentsForProfile(
      Profile* profile) override;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
