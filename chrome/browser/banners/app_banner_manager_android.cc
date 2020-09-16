// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/AppBannerManager_jni.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/android/webapps/add_to_homescreen_coordinator.h"
#include "chrome/browser/android/webapps/add_to_homescreen_params.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/version_info/channel.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace {

constexpr char kPlatformPlay[] = "play";

// Whether to ignore the Chrome channel in QueryNativeApp() for testing.
bool gIgnoreChromeChannelForTesting = false;

// Returns a pointer to the InstallableAmbientBadgeInfoBar if it is currently
// showing. Otherwise returns nullptr.
infobars::InfoBar* GetVisibleAmbientBadgeInfoBar(
    InfoBarService* infobar_service) {
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        InstallableAmbientBadgeInfoBarDelegate::
            INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE) {
      return infobar;
    }
  }
  return nullptr;
}

bool CanShowAppBanners(TabAndroid* tab) {
  if (!tab)
    return false;
  return static_cast<android::TabWebContentsDelegateAndroid*>(
             tab->web_contents()->GetDelegate())
      ->CanShowAppBanners();
}

}  // anonymous namespace

namespace banners {

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
      ShortcutHelper::GetIdealHomescreenIconSizeInPx(),
      ShortcutHelper::GetMinimumHomescreenIconSizeInPx(),
      base::BindOnce(&AppBannerManagerAndroid::OnNativeAppIconFetched,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isEnabledForTab(env, java_banner_manager_))
    return;

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (!CanShowAppBanners(tab))
    return;

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
  banners::TrackDismissEvent(banners::DISMISS_EVENT_AMBIENT_INFOBAR_DISMISSED);

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

bool AppBannerManagerAndroid::IsWebAppConsideredInstalled() {
  // Also check if a WebAPK is currently being installed. Installation may take
  // some time, so ensure we don't accidentally allow a new installation whilst
  // one is in flight for the current site.
  return AppBannerManager::IsWebAppConsideredInstalled() ||
         WebApkInstallService::Get(web_contents()->GetBrowserContext())
             ->IsInstallInProgress(manifest_url_);
}

InstallableParams
AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableWebAppCheck();
  params.prefer_maskable_icon =
      ShortcutHelper::DoesAndroidSupportMaskableIcons();

  return params;
}

void AppBannerManagerAndroid::PerformInstallableChecks() {
  if (ShouldPerformInstallableNativeAppCheck())
    PerformInstallableNativeAppCheck();
  else
    PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::PerformInstallableWebAppCheck() {
  if (!AreWebManifestUrlsWebApkCompatible(manifest_)) {
    Stop(URL_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }
  AppBannerManager::PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  if (data.errors.empty())
    WebApkUkmRecorder::RecordWebApkableVisit(data.manifest_url);

  AppBannerManager::OnDidPerformInstallableWebAppCheck(data);
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  auto a2hs_params = std::make_unique<AddToHomescreenParams>();
  a2hs_params->primary_icon = primary_icon_;
  if (native_app_data_.is_null()) {
    a2hs_params->app_type = AddToHomescreenParams::AppType::WEBAPK;
    a2hs_params->shortcut_info = ShortcutHelper::CreateShortcutInfo(
        manifest_url_, manifest_, primary_icon_url_);
    a2hs_params->install_source = install_source;
    a2hs_params->has_maskable_primary_icon = has_maskable_primary_icon_;
  } else {
    a2hs_params->app_type = AddToHomescreenParams::AppType::NATIVE;
    a2hs_params->native_app_data = native_app_data_;
    a2hs_params->native_app_package_name = native_app_package_;
  }

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

    case AddToHomescreenInstaller::Event::UI_DISMISSED:
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

  banners::TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

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
      ShortcutHelper::GetIdealHomescreenIconSizeInPx());
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

void AppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (!base::FeatureList::IsEnabled(features::kInstallableAmbientBadgeInfoBar))
    return;

  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents(), validated_url_, GetAppIdentifier(),
          GetCurrentTime())) {
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (infobar_service == nullptr)
    return;

  if (GetVisibleAmbientBadgeInfoBar(infobar_service) == nullptr) {
    InstallableAmbientBadgeInfoBarDelegate::Create(
        web_contents(), weak_factory_.GetWeakPtr(), GetAppName(), primary_icon_,
        has_maskable_primary_icon_, manifest_.start_url);
  }
}

void AppBannerManagerAndroid::HideAmbientBadge() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (infobar_service == nullptr)
    return;

  infobars::InfoBar* ambient_badge_infobar =
      GetVisibleAmbientBadgeInfoBar(infobar_service);

  if (ambient_badge_infobar)
    infobar_service->RemoveInfoBar(ambient_badge_infobar);
}

bool AppBannerManagerAndroid::IsSupportedAppPlatform(
    const base::string16& platform) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsRelatedAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
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

}  // namespace banners
