// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

using content::BrowserThread;
using content::NavigationHandle;
using content::NavigationThrottle;

namespace {

const RE2& GetFlashURLCanonicalRegex() {
  static const base::NoDestructor<RE2> re("(?i)get2?\\.adobe\\.com/.*flash.*");
  return *re;
}

const RE2& GetFlashURLSecondaryGoRegex() {
  static const base::NoDestructor<RE2> re(
      "(?i)(www\\.)?(adobe|macromedia)\\.com/go/"
      "((?i).*get[-_]?flash|getfp10android|.*fl(ash)player|.*flashpl|"
      ".*flash_player|flash_completion|flashpm|.*flashdownload|d65_flplayer|"
      "fp_jp|runtimes_fp|[a-z_-]{3,6}h-m-a-?2|chrome|download_player|"
      "gnav_fl|pdcredirect).*");
  return *re;
}

const RE2& GetFlashURLSecondaryDownloadRegex() {
  static const base::NoDestructor<RE2> re(
      "(?i)(www\\.)?(adobe|macromedia)\\.com/shockwave/download/download.cgi");
  return *re;
}

const char kGetFlashURLSecondaryDownloadQuery[] =
    "P1_Prod_Version=ShockwaveFlash";

bool InterceptNavigation(
    const GURL& source_url,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  FlashDownloadInterception::InterceptFlashDownloadNavigation(source,
                                                              source_url);
  return true;
}

}  // namespace

// static
void FlashDownloadInterception::InterceptFlashDownloadNavigation(
    content::WebContents* web_contents,
    const GURL& source_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
      host_content_settings_map, url::Origin::Create(source_url), source_url,
      nullptr);

  if (flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT) {
    PermissionManager* manager = PermissionManager::Get(profile);
    manager->RequestPermission(
        ContentSettingsType::PLUGINS, web_contents->GetMainFrame(),
        web_contents->GetLastCommittedURL(), true, base::DoNothing());
  } else if (flash_setting == CONTENT_SETTING_BLOCK) {
    auto* settings = TabSpecificContentSettings::FromWebContents(web_contents);
    if (settings)
      settings->FlashDownloadBlocked();
  }

  // If the content setting has been already changed, do nothing.
}

// static
bool FlashDownloadInterception::ShouldStopFlashDownloadAction(
    HostContentSettingsMap* host_content_settings_map,
    const GURL& source_url,
    const GURL& target_url,
    bool has_user_gesture) {
  if (!has_user_gesture)
    return false;

  url::Replacements<char> replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();

  // If the navigation source is already the Flash download page, don't
  // intercept the download. The user may be trying to download Flash.
  std::string source_url_str =
      source_url.ReplaceComponents(replacements).GetContent();

  std::string target_url_str =
      target_url.ReplaceComponents(replacements).GetContent();

  // Early optimization since RE2 is expensive. http://crbug.com/809775
  if (target_url_str.find("adobe.com") == std::string::npos &&
      target_url_str.find("macromedia.com") == std::string::npos)
    return false;

  if (RE2::PartialMatch(source_url_str, GetFlashURLCanonicalRegex()))
    return false;

  if (RE2::FullMatch(target_url_str, GetFlashURLCanonicalRegex()) ||
      RE2::FullMatch(target_url_str, GetFlashURLSecondaryGoRegex()) ||
      (RE2::FullMatch(target_url_str, GetFlashURLSecondaryDownloadRegex()) &&
       target_url.query() == kGetFlashURLSecondaryDownloadQuery)) {
    ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
        host_content_settings_map, url::Origin::Create(source_url), source_url,
        nullptr);

    return flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT ||
           flash_setting == CONTENT_SETTING_BLOCK;
  }

  return false;
}

// static
std::unique_ptr<NavigationThrottle>
FlashDownloadInterception::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Browser initiated navigations like the Back button or the context menu
  // should never be intercepted.
  if (!handle->IsRendererInitiated())
    return nullptr;

  // The source URL may be empty, it's a new tab. Intercepting that navigation
  // would lead to a blank new tab, which would be bad.
  GURL source_url = handle->GetWebContents()->GetLastCommittedURL();
  if (source_url.is_empty())
    return nullptr;

  // Always treat main-frame navigations as having a user gesture. We have to do
  // this because the user gesture system can be foiled by popular JavaScript
  // analytics frameworks that capture the click event. crbug.com/678097
  bool has_user_gesture = handle->HasUserGesture() || handle->IsInMainFrame();

  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!ShouldStopFlashDownloadAction(host_content_settings_map, source_url,
                                     handle->GetURL(), has_user_gesture)) {
    return nullptr;
  }

  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      handle, base::Bind(&InterceptNavigation, source_url),
      navigation_interception::SynchronyMode::kSync);
}
