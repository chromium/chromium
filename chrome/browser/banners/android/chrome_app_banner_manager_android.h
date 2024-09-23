// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_
#define CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"

namespace content {
class WebContents;
}

namespace segmentation_platform {
class SegmentationPlatformService;
}

namespace webapps {

// Extends the AppBannerManagerAndroid with some Chrome-specific alternative UI
// paths, including in product help (IPH) and the PWA bottom sheet.
class ChromeAppBannerManagerAndroid
    : public AppBannerManagerAndroid::ChromeDelegate {
 public:
  explicit ChromeAppBannerManagerAndroid(content::WebContents& web_contents);
  ChromeAppBannerManagerAndroid(const ChromeAppBannerManagerAndroid&) = delete;
  ChromeAppBannerManagerAndroid& operator=(
      const ChromeAppBannerManagerAndroid&) = delete;
  ~ChromeAppBannerManagerAndroid() override;

 protected:
  // AppBannerManagerAndroid::ChromeDelegate:
  void OnInstallableCheckedNoErrors(
      const ManifestId& manifest_id) const override;
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService() override;
  PrefService* GetPrefService() override;
  void RecordExtraMetricsForInstallEvent(
      AddToHomescreenInstaller::Event event,
      const AddToHomescreenParams& a2hs_params) override;

 private:
  // This class is owned by a class that is a WebContentsUserData, so this is
  // safe.
  raw_ref<content::WebContents> web_contents_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_
