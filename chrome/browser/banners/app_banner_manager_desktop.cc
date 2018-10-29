// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "extensions/common/constants.h"

namespace {

bool gDisableTriggeringForTesting = false;

}  // namespace

namespace banners {

bool AppBannerManagerDesktop::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kAppBanners) ||
         IsExperimentalAppBannersEnabled();
}

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return AppBannerManagerDesktop::FromWebContents(web_contents);
}

void AppBannerManagerDesktop::DisableTriggeringForTesting() {
  gDisableTriggeringForTesting = true;
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) { }

AppBannerManagerDesktop::~AppBannerManagerDesktop() { }

void AppBannerManagerDesktop::DidFinishCreatingBookmarkApp(
    const extensions::Extension* extension,
    const WebApplicationInfo& web_app_info) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  if (extension) {
    SendBannerAccepted();
    AppBannerSettingsHelper::RecordBannerInstallEvent(
        contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);

    // OnInstall must be called last since it resets Mojo bindings.
    OnInstall(false /* is_native app */, blink::kWebDisplayModeStandalone);
    return;
  }

  // |extension| is null, so we assume that the confirmation dialog was
  // cancelled. Alternatively, the extension installation may have failed, but
  // we can't tell the difference here.
  // TODO(crbug.com/789381): plumb through enough information to be able to
  // distinguish between extension install failures and user-cancellations of
  // the app install dialog.
  SendBannerDismissed();
  TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
  AppBannerSettingsHelper::RecordBannerDismissEvent(
      contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);
}

bool AppBannerManagerDesktop::IsWebAppConsideredInstalled(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const GURL& start_url,
    const GURL& manifest_url) {
  return extensions::BookmarkOrHostedAppInstalled(
      web_contents->GetBrowserContext(), start_url);
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents && !manifest_.IsEmpty());

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  WebApplicationInfo web_app_info;

  bookmark_app_helper_.reset(new extensions::BookmarkAppHelper(
      profile, web_app_info, contents, install_source));

  if (IsExperimentalAppBannersEnabled()) {
    RecordDidShowBanner("AppBanner.WebApp.Shown");
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    ReportStatus(SHOWING_APP_INSTALLATION_DIALOG);
    bookmark_app_helper_->Create(base::Bind(
        &AppBannerManager::DidFinishCreatingBookmarkApp, GetWeakPtr()));
    return;
  }

  // This differs from Android, where there is a concrete
  // AppBannerInfoBarAndroid class to interface with Java, and the manager calls
  // the InfoBarService to show the banner. On desktop, an InfoBar class
  // is not required, and the delegate calls the InfoBarService.
  infobars::InfoBar* infobar = AppBannerInfoBarDelegateDesktop::Create(
      contents, GetWeakPtr(), bookmark_app_helper_.get(), manifest_);
  if (infobar) {
    RecordDidShowBanner("AppBanner.WebApp.Shown");
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
    ReportStatus(SHOWING_WEB_APP_BANNER);
  } else {
    ReportStatus(FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::DidFinishLoad(render_frame_host, validated_url);
}

void AppBannerManagerDesktop::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType type) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::OnEngagementEvent(web_contents, url, score, type);
}

}  // namespace banners
