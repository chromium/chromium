// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/android/webapps/add_to_homescreen_coordinator.h"
#include "chrome/browser/android/webapps/add_to_homescreen_params.h"
#include "chrome/browser/banners/android/jni_headers/AppBannerInProductHelpControllerProvider_jni.h"
#include "chrome/browser/banners/android/jni_headers/AppBannerManager_jni.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/webapps/android/features.h"
#include "chrome/browser/webapps/android/pwa_bottom_sheet_controller.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/version_info/channel.h"
#include "components/webapps/android/shortcut_info.h"
#include "components/webapps/android/webapps_icon_utils.h"
#include "components/webapps/android/webapps_utils.h"
#include "components/webapps/installable/installable_data.h"
#include "components/webapps/installable/installable_metrics.h"
#include "components/webapps/webapps_client.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace webapps {

namespace {

constexpr char kPlatformPlay[] = "play";

// The key to look up what the minimum engagement score is for showing the
// in-product help.
constexpr char kMinEngagementForIphKey[] = "x_min_engagement_for_iph";

// The key to look up whether the in-product help should replace the toolbar or
// complement it.
constexpr char kIphReplacesToolbar[] = "x_iph_replaces_toolbar";

// Whether to ignore the Chrome channel in QueryNativeApp() for testing.
bool gIgnoreChromeChannelForTesting = false;

}  // anonymous namespace

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
  CreateJavaBannerManager(web_contents);
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_);
  java_banner_manager_.Reset();
}

const base::android::ScopedJavaLocalRef<jobject>
AppBannerManagerAndroid::GetJavaBannerManager() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_banner_manager_);
}

bool AppBannerManagerAndroid::IsRunningForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsRunning();
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  UpdateState(State::ACTIVE);
  native_app_data_.Reset(japp_data);
  native_app_title_ = ConvertJavaStringToUTF16(env, japp_title);
  native_app_package_ = ConvertJavaStringToUTF8(env, japp_package);
  primary_icon_url_ = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  if (!CheckIfShouldShowBanner())
    return false;

  return content::ManifestIconDownloader::Download(
      web_contents(), primary_icon_url_,
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx(),
      WebappsIconUtils::GetMinimumHomescreenIconSizeInPx(),
      base::BindOnce(&AppBannerManagerAndroid::OnNativeAppIconFetched,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isSupported(env) ||
      !WebappsClient::Get()->CanShowAppBanners(web_contents())) {
    return;
  }

  AppBannerManager::RequestAppBanner(validated_url);
}

void AppBannerManagerAndroid::AddToHomescreenFromBadge() {
  ShowBannerUi(InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE));

  // Close our bindings to ensure that any existing beforeinstallprompt events
  // cannot trigger add to home screen (which would cause a crash). If the
  // banner is dismissed, the event will be resent.
  ResetBindings();
}

void AppBannerManagerAndroid::BadgeDismissed() {
  TrackDismissEvent(DISMISS_EVENT_AMBIENT_INFOBAR_DISMISSED);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, GetCurrentTime());
}

std::string AppBannerManagerAndroid::GetAppIdentifier() {
  return native_app_data_.is_null() ? AppBannerManager::GetAppIdentifier()
                                    : native_app_package_;
}

std::string AppBannerManagerAndroid::GetBannerType() {
  return native_app_data_.is_null() ? AppBannerManager::GetBannerType()
                                    : "play";
}

InstallableParams
AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableWebAppCheck();
  params.prefer_maskable_icon =
      WebappsIconUtils::DoesAndroidSupportMaskableIcons();
  if (base::FeatureList::IsEnabled(
          webapps::features::kPwaInstallUseBottomSheet))
    params.fetch_screenshots = true;

  return params;
}

void AppBannerManagerAndroid::PerformInstallableChecks() {
  if (ShouldPerformInstallableNativeAppCheck())
    PerformInstallableNativeAppCheck();
  else
    PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::PerformInstallableWebAppCheck() {
  if (!webapps::WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest_)) {
    Stop(URL_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }
  AppBannerManager::PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  if (data.NoBlockingErrors())
    WebApkUkmRecorder::RecordWebApkableVisit(data.manifest_url);
  screenshots_ = data.screenshots;

  AppBannerManager::OnDidPerformInstallableWebAppCheck(data);
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
}

std::unique_ptr<AddToHomescreenParams>
AppBannerManagerAndroid::CreateAddToHomescreenParams(
    WebappInstallSource install_source) {
  auto a2hs_params = std::make_unique<AddToHomescreenParams>();
  a2hs_params->primary_icon = primary_icon_;
  if (native_app_data_.is_null()) {
    a2hs_params->app_type = AddToHomescreenParams::AppType::WEBAPK;
    a2hs_params->shortcut_info = ShortcutInfo::CreateShortcutInfo(
        manifest_url_, manifest_, primary_icon_url_);
    a2hs_params->install_source = install_source;
    a2hs_params->has_maskable_primary_icon = has_maskable_primary_icon_;
  } else {
    a2hs_params->app_type = AddToHomescreenParams::AppType::NATIVE;
    a2hs_params->native_app_data = native_app_data_;
    a2hs_params->native_app_package_name = native_app_package_;
  }
  return a2hs_params;
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  auto a2hs_params = CreateAddToHomescreenParams(install_source);

  bool was_shown = AddToHomescreenCoordinator::ShowForAppBanner(
      weak_factory_.GetWeakPtr(), std::move(a2hs_params),
      base::BindRepeating(&AppBannerManagerAndroid::RecordEventForAppBanner,
                          weak_factory_.GetWeakPtr()));

  // If we are installing from the ambient badge, it will remove itself.
  if (install_source != WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB &&
      install_source != WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB) {
    HideAmbientBadge();
  }

  if (was_shown) {
    if (native_app_data_.is_null()) {
      ReportStatus(SHOWING_WEB_APP_BANNER);
    } else {
      ReportStatus(SHOWING_NATIVE_APP_BANNER);
    }
  } else {
    ReportStatus(FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerAndroid::RecordEventForAppBanner(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  switch (event) {
    case AddToHomescreenInstaller::Event::INSTALL_STARTED:
      TrackDismissEvent(DISMISS_EVENT_DISMISSED);
      switch (a2hs_params.app_type) {
        case AddToHomescreenParams::AppType::NATIVE:
          TrackUserResponse(USER_RESPONSE_NATIVE_APP_ACCEPTED);
          break;
        case AddToHomescreenParams::AppType::WEBAPK:
          FALLTHROUGH;
        case AddToHomescreenParams::AppType::SHORTCUT:
          TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
          AppBannerSettingsHelper::RecordBannerInstallEvent(
              web_contents(), a2hs_params.shortcut_info->url.spec());
          break;
        default:
          NOTREACHED();
      }
      break;

    case AddToHomescreenInstaller::Event::INSTALL_FAILED:
      TrackDismissEvent(DISMISS_EVENT_ERROR);
      break;

    case AddToHomescreenInstaller::Event::NATIVE_INSTALL_OR_OPEN_FAILED:
      DCHECK_EQ(a2hs_params.app_type, AddToHomescreenParams::AppType::NATIVE);
      TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED);
      break;

    case AddToHomescreenInstaller::Event::NATIVE_INSTALL_OR_OPEN_SUCCEEDED:
      DCHECK_EQ(a2hs_params.app_type, AddToHomescreenParams::AppType::NATIVE);
      TrackDismissEvent(DISMISS_EVENT_APP_OPEN);
      break;

    case AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED:
      SendBannerAccepted();
      if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK ||
          a2hs_params.app_type == AddToHomescreenParams::AppType::SHORTCUT) {
        OnInstall(a2hs_params.shortcut_info->display);
      }
      break;

    case AddToHomescreenInstaller::Event::NATIVE_DETAILS_SHOWN:
      TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
      break;

    case AddToHomescreenInstaller::Event::UI_SHOWN:
      if (a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE) {
        RecordDidShowBanner();
        TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED);
      } else {
        RecordDidShowBanner();
        TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
      }
      break;

    case AddToHomescreenInstaller::Event::UI_CANCELLED:
      TrackDismissEvent(DISMISS_EVENT_DISMISSED);

      SendBannerDismissed();
      if (a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE) {
        DCHECK(!a2hs_params.native_app_package_name.empty());
        TrackUserResponse(USER_RESPONSE_NATIVE_APP_DISMISSED);
        AppBannerSettingsHelper::RecordBannerDismissEvent(
            web_contents(), a2hs_params.native_app_package_name);
      } else {
        if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK)
          webapk::TrackInstallEvent(
              webapk::ADD_TO_HOMESCREEN_DIALOG_DISMISSED_BEFORE_INSTALLATION);
        TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
        AppBannerSettingsHelper::RecordBannerDismissEvent(
            web_contents(), a2hs_params.shortcut_info->url.spec());
      }
      break;
  }
}

void AppBannerManagerAndroid::CreateJavaBannerManager(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_banner_manager_.Reset(
      Java_AppBannerManager_create(env, reinterpret_cast<intptr_t>(this)));
}

std::string AppBannerManagerAndroid::ExtractQueryValueForName(
    const GURL& url,
    const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return it.GetValue();
  }
  return std::string();
}

bool AppBannerManagerAndroid::ShouldPerformInstallableNativeAppCheck() {
  if (!manifest_.prefer_related_applications || java_banner_manager_.is_null())
    return false;

  // Ensure there is at least one related app specified that is supported on
  // the current platform.
  for (const auto& application : manifest_.related_applications) {
    if (base::EqualsASCII(application.platform.value_or(base::string16()),
                          kPlatformPlay))
      return true;
  }
  return false;
}

void AppBannerManagerAndroid::PerformInstallableNativeAppCheck() {
  DCHECK(ShouldPerformInstallableNativeAppCheck());
  InstallableStatusCode code = NO_ERROR_DETECTED;
  for (const auto& application : manifest_.related_applications) {
    std::string id =
        base::UTF16ToUTF8(application.id.value_or(base::string16()));
    code = QueryNativeApp(application.platform.value_or(base::string16()),
                          application.url, id);
    if (code == NO_ERROR_DETECTED)
      return;
  }

  // We must have some error in |code| if we reached this point, so report it.
  Stop(code);
}

InstallableStatusCode AppBannerManagerAndroid::QueryNativeApp(
    const base::string16& platform,
    const GURL& url,
    const std::string& id) {
  if (!base::EqualsASCII(platform, kPlatformPlay))
    return PLATFORM_NOT_SUPPORTED_ON_ANDROID;

  if (id.empty())
    return NO_ID_SPECIFIED;

  // AppBannerManager#fetchAppDetails() only works on Beta and Stable because
  // the called Google Play API uses an old way of checking whether the Chrome
  // app is first party. See http://b/147780265
  version_info::Channel channel = chrome::GetChannel();
  if (!gIgnoreChromeChannelForTesting &&
      !(channel == version_info::Channel::BETA ||
        channel == version_info::Channel::STABLE)) {
    return PREFER_RELATED_APPLICATIONS_SUPPORTED_ONLY_BETA_STABLE;
  }

  TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  std::string id_from_app_url = ExtractQueryValueForName(url, "id");
  if (id_from_app_url.size() && id != id_from_app_url)
    return IDS_DO_NOT_MATCH;

  // Attach the chrome_inline referrer value, prefixed with "&" if the
  // referrer is non empty.
  std::string referrer = ExtractQueryValueForName(url, "referrer");
  if (!referrer.empty())
    referrer += "&";
  referrer += "playinline=chrome_inline";

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url_.spec()));
  base::android::ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  base::android::ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));

  // This async call will run OnAppDetailsRetrieved() when completed.
  UpdateState(State::FETCHING_NATIVE_DATA);
  Java_AppBannerManager_fetchAppDetails(
      env, java_banner_manager_, jurl, jpackage, jreferrer,
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx());
  return NO_ERROR_DETECTED;
}

void AppBannerManagerAndroid::OnNativeAppIconFetched(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    Stop(NO_ICON_AVAILABLE);
    return;
  }

  primary_icon_ = bitmap;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementEvent to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

base::string16 AppBannerManagerAndroid::GetAppName() const {
  if (native_app_data_.is_null()) {
    // Prefer the short name if it's available. It's guaranteed that at least
    // one of these is non-empty.
    base::string16 short_name = manifest_.short_name.value_or(base::string16());
    return short_name.empty() ? manifest_.name.value_or(base::string16())
                              : short_name;
  }

  return native_app_title_;
}

bool AppBannerManagerAndroid::MaybeShowInProductHelp() const {
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

void AppBannerManagerAndroid::Install() {
  WebappInstallSource install_source = InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE);
  auto params = CreateAddToHomescreenParams(install_source);
  AddToHomescreenInstaller::Install(
      web_contents(), *params,
      base::BindRepeating(&AppBannerManagerAndroid::RecordEventForAppBanner,
                          weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (MaybeShowInProductHelp() &&
      base::GetFieldTrialParamByFeatureAsBool(
          feature_engagement::kIPHPwaInstallAvailableFeature,
          kIphReplacesToolbar, false)) {
    DVLOG(2) << "Install infobar overridden by IPH, as per experiment.";
    return;
  }

  if (!base::FeatureList::IsEnabled(
          ::features::kInstallableAmbientBadgeInfoBar))
    return;

  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents(), validated_url_, GetAppIdentifier(),
          GetCurrentTime())) {
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  bool infobar_visible =
      infobar_service &&
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_service);

  if (!infobar_visible) {
    PwaBottomSheetController::MaybeCreateAndShow(
        weak_factory_.GetWeakPtr(), web_contents(), GetAppName(), primary_icon_,
        has_maskable_primary_icon_, manifest_.start_url, screenshots_,
        manifest_.description.value_or(base::string16()), manifest_.categories,
        /* show_expaned= */ false);
  }
}

void AppBannerManagerAndroid::HideAmbientBadge() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (infobar_service == nullptr)
    return;

  infobars::InfoBar* ambient_badge_infobar =
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_service);

  if (ambient_badge_infobar)
    infobar_service->RemoveInfoBar(ambient_badge_infobar);
}

bool AppBannerManagerAndroid::IsSupportedNonWebAppPlatform(
    const base::string16& platform) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsRelatedNonWebAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsWebAppConsideredInstalled() const {
  // Also check if a WebAPK is currently being installed. Installation may take
  // some time, so ensure we don't accidentally allow a new installation whilst
  // one is in flight for the current site.
  return WebappsUtils::IsWebApkInstalled(web_contents()->GetBrowserContext(),
                                         manifest_.start_url) ||
         WebappsClient::Get()->IsInstallationInProgress(web_contents(),
                                                        manifest_url_);
}

base::WeakPtr<AppBannerManager> AppBannerManagerAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerAndroid::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return AppBannerManagerAndroid::FromWebContents(web_contents);
}

// static
base::android::ScopedJavaLocalRef<jobject>
JNI_AppBannerManager_GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  AppBannerManagerAndroid* manager = AppBannerManagerAndroid::FromWebContents(
      content::WebContents::FromJavaWebContents(java_web_contents));
  return manager ? manager->GetJavaBannerManager()
                 : base::android::ScopedJavaLocalRef<jobject>();
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_AppBannerManager_GetInstallableWebAppName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  return base::android::ConvertUTF16ToJavaString(
      env, AppBannerManager::GetInstallableWebAppName(
               content::WebContents::FromJavaWebContents(java_web_contents)));
}

// static
void JNI_AppBannerManager_IgnoreChromeChannelForTesting(JNIEnv*) {
  gIgnoreChromeChannelForTesting = true;
}

// static
void JNI_AppBannerManager_SetDaysAfterDismissAndIgnoreToTrigger(
    JNIEnv* env,
    jint dismiss_days,
    jint ignore_days) {
  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(dismiss_days,
                                                                 ignore_days);
}

// static
void JNI_AppBannerManager_SetTimeDeltaForTesting(JNIEnv* env, jint days) {
  AppBannerManager::SetTimeDeltaForTesting(days);
}

// static
void JNI_AppBannerManager_SetTotalEngagementToTrigger(JNIEnv* env,
                                                      jdouble engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerAndroid)

}  // namespace webapps
