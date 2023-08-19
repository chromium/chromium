// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_
#define CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_

#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"

class PrefService;

namespace segmentation_platform {
class SegmentationPlatformService;
}

namespace webapps {

// Extends the AppBannerManagerAndroid with some Chrome-specific alternative UI
// paths, including in product help (IPH) and the PWA bottom sheet.
class ChromeAppBannerManagerAndroid
    : public AppBannerManagerAndroid,
      public content::WebContentsUserData<ChromeAppBannerManagerAndroid> {
 public:
  explicit ChromeAppBannerManagerAndroid(content::WebContents* web_contents);
  ChromeAppBannerManagerAndroid(const ChromeAppBannerManagerAndroid&) = delete;
  ChromeAppBannerManagerAndroid& operator=(
      const ChromeAppBannerManagerAndroid&) = delete;
  ~ChromeAppBannerManagerAndroid() override;

  using content::WebContentsUserData<
      ChromeAppBannerManagerAndroid>::FromWebContents;

 protected:
  // AppBannerManagerAndroid:
  void OnDidPerformInstallableWebAppCheck(
      const InstallableData& result) override;
  void MaybeShowAmbientBadge() override;
  void RecordExtraMetricsForInstallEvent(
      AddToHomescreenInstaller::Event event,
      const AddToHomescreenParams& a2hs_params) override;
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService() override;

 private:
  friend class content::WebContentsUserData<ChromeAppBannerManagerAndroid>;

  // Shows the in-product help if possible and returns true when a request to
  // show it was made, but false if conditions (e.g. engagement score) for
  // showing where not deemed adequate.
  bool MaybeShowInProductHelp() const;

  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  raw_ptr<PrefService> pref_service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_ANDROID_CHROME_APP_BANNER_MANAGER_ANDROID_H_
