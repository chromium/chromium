// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_permission_context.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

BackgroundFetchPermissionContext::BackgroundFetchPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::BACKGROUND_FETCH,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

ContentSetting BackgroundFetchPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (render_frame_host && !render_frame_host->GetParent()) {
    DownloadRequestLimiter* limiter =
        g_browser_process->download_request_limiter();
    DCHECK(limiter);
    auto status = limiter->GetDownloadStatus(
        content::WebContents::FromRenderFrameHost(render_frame_host));

    switch (status) {
      case DownloadRequestLimiter::DownloadStatus::ALLOW_ONE_DOWNLOAD:
      case DownloadRequestLimiter::DownloadStatus::ALLOW_ALL_DOWNLOADS:
        return CONTENT_SETTING_ALLOW;
      case DownloadRequestLimiter::DownloadStatus::PROMPT_BEFORE_DOWNLOAD:
        return CONTENT_SETTING_ASK;
      case DownloadRequestLimiter::DownloadStatus::DOWNLOADS_NOT_ALLOWED:
        return CONTENT_SETTING_BLOCK;
    }

    NOTREACHED_IN_MIGRATION();
  }

  // |render_frame_host| is either a nullptr, which means we're being called
  // from a worker context, or it's not a top level frame. In either case, use
  // content settings.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(host_content_settings_map);

  // The set of valid settings for automatic downloads is defined as
  // {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK}.
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      requesting_origin, requesting_origin,
      ContentSettingsType::AUTOMATIC_DOWNLOADS);

  // Due to privacy concerns as outlined in https://crbug.com/896311 the most
  // permissive state BGF can be in for non top level frames or service workers
  // is ASK. This causes background fetches that do not originate in a main
  // frame to start paused.
  if (setting == CONTENT_SETTING_ALLOW)
    setting = CONTENT_SETTING_ASK;

  return setting;
}

void BackgroundFetchPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize Background Fetch
  // from BackgroundFetchPermissionContext.
  // BackgroundFetchDelegateImpl invokes CanDownload() on DownloadRequestLimiter
  // to prompt the user.
  NOTREACHED_IN_MIGRATION();
}

void BackgroundFetchPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(!persist);
  DCHECK(is_final_decision);

  permissions::PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, is_one_time, is_final_decision);
}
