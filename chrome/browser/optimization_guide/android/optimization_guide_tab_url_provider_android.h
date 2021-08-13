// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_

#include "components/optimization_guide/core/tab_url_provider.h"

class Profile;

namespace optimization_guide {
namespace android {

// Implementation of OptimizationGuideTabUrlProvider that gets URLs from Android
// browser windows.
class OptimizationGuideTabUrlProviderAndroid
    : public optimization_guide::TabUrlProvider {
 public:
  explicit OptimizationGuideTabUrlProviderAndroid(Profile* profile);
  ~OptimizationGuideTabUrlProviderAndroid() override;

  // optimization_guide::TabUrlProvider:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override;

 private:
  // The profile associated with this tab URL provider.
  Profile* profile_;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
