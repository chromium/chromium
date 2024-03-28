// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/android/bottomsheet/pwa_bottom_sheet_controller.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace webapps {

ChromeAppBannerManagerAndroid::ChromeAppBannerManagerAndroid(
    content::WebContents& web_contents)
    : web_contents_(web_contents) {}

ChromeAppBannerManagerAndroid::~ChromeAppBannerManagerAndroid() = default;

void ChromeAppBannerManagerAndroid::OnInstallableCheckedNoErrors(
    const ManifestId& manifest_id) const {
  // TODO(b/320681613): Maybe move this to components.
  webapk::WebApkUkmRecorder::RecordWebApkableVisit(manifest_id);
}

segmentation_platform::SegmentationPlatformService*
ChromeAppBannerManagerAndroid::GetSegmentationPlatformService() {
  return segmentation_platform::SegmentationPlatformServiceFactory::
      GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

PrefService* ChromeAppBannerManagerAndroid::GetPrefService() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}

void ChromeAppBannerManagerAndroid::RecordExtraMetricsForInstallEvent(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK &&
      event == AddToHomescreenInstaller::Event::UI_CANCELLED) {
    // TODO(b/320681613): Maybe move this to components.
    webapk::TrackInstallEvent(
        webapk::ADD_TO_HOMESCREEN_DIALOG_DISMISSED_BEFORE_INSTALLATION);
  }
}

}  // namespace webapps
