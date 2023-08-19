// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/banners/android/jni_headers/AppBannerInProductHelpControllerProvider_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/site_engagement/content/site_engagement_service.h"
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

namespace {

// The key to look up what the minimum engagement score is for showing the
// in-product help.
constexpr char kMinEngagementForIphKey[] = "x_min_engagement_for_iph";

// The key to look up whether the in-product help should replace the toolbar or
// complement it.
constexpr char kIphReplacesToolbar[] = "x_iph_replaces_toolbar";

}  // anonymous namespace

ChromeAppBannerManagerAndroid::ChromeAppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManagerAndroid(web_contents),
      content::WebContentsUserData<ChromeAppBannerManagerAndroid>(
          *web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  segmentation_platform_service_ =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile);

  pref_service_ = profile->GetPrefs();
}

ChromeAppBannerManagerAndroid::~ChromeAppBannerManagerAndroid() = default;

void ChromeAppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  if (data.errors.empty()) {
    WebApkUkmRecorder::RecordWebApkableVisit(*data.manifest_url);
  }

  AppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(data);
}

void ChromeAppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (MaybeShowInProductHelp()) {
    TrackIphWasShown();
    if (base::GetFieldTrialParamByFeatureAsBool(
            feature_engagement::kIPHPwaInstallAvailableFeature,
            kIphReplacesToolbar, false)) {
      DVLOG(2) << "Install infobar overridden by IPH, as per experiment.";
      return;
    }
  }

  ambient_badge_manager_ = std::make_unique<AmbientBadgeManager>(
      web_contents(), GetAndroidWeakPtr(), segmentation_platform_service_,
      pref_service_);
  ambient_badge_manager_->MaybeShow(
      validated_url_, GetAppName(), GetAppIdentifier(),
      CreateAddToHomescreenParams(InstallableMetrics::GetInstallSource(
          web_contents(), InstallTrigger::AMBIENT_BADGE)),
      base::BindOnce(&ChromeAppBannerManagerAndroid::ShowBannerFromBadge,
                     GetAndroidWeakPtr()));
}

void ChromeAppBannerManagerAndroid::RecordExtraMetricsForInstallEvent(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK &&
      event == AddToHomescreenInstaller::Event::UI_CANCELLED) {
    webapk::TrackInstallEvent(
        webapk::ADD_TO_HOMESCREEN_DIALOG_DISMISSED_BEFORE_INSTALLATION);
  }
}

segmentation_platform::SegmentationPlatformService*
ChromeAppBannerManagerAndroid::GetSegmentationPlatformService() {
  // TODO(https://crbug.com/1449993): Implement.
  // Note: By returning a non-nullptr, all of the Ml code (after metrics
  // gathering) in `MlInstallabilityPromoter` will execute, including requesting
  // classifiction & eventually calling `OnMlInstallPrediction` above. Make sure
  // that the contract of that class is being followed appropriately, and the ML
  // parts are correct.
  return nullptr;
}

bool ChromeAppBannerManagerAndroid::MaybeShowInProductHelp() const {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHPwaInstallAvailableFeature)) {
    DVLOG(2) << "Feature not enabled";
    return false;
  }

  if (!web_contents()) {
    DVLOG(2) << "IPH for PWA aborted: null WebContents";
    return false;
  }

  double last_engagement_score =
      GetSiteEngagementService()->GetScore(validated_url_);
  int min_engagement = base::GetFieldTrialParamByFeatureAsInt(
      feature_engagement::kIPHPwaInstallAvailableFeature,
      kMinEngagementForIphKey, 0);
  if (last_engagement_score < min_engagement) {
    DVLOG(2) << "IPH for PWA aborted: Engagement score too low: "
             << last_engagement_score << " < " << min_engagement;
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::string error_message = base::android::ConvertJavaStringToUTF8(
      Java_AppBannerInProductHelpControllerProvider_showInProductHelp(
          env, web_contents()->GetJavaWebContents()));
  if (!error_message.empty()) {
    DVLOG(2) << "IPH for PWA showing aborted. " << error_message;
    return false;
  }

  DVLOG(2) << "Showing IPH.";
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAppBannerManagerAndroid);

}  // namespace webapps
