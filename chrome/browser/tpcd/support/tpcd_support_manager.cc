// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/tpcd_support_manager.h"

#include <memory>
#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

/* static */
void TpcdSupportManager::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kPersistentOriginTrials)) {
    return;
  }

  // TODO (crbug.com/1466156): condition creation on the type of profile
  // associated with the WebContents.
  TpcdSupportManager::CreateForWebContents(
      web_contents,
      std::make_unique<TpcdSupportDelegate>(web_contents->GetBrowserContext()));
}

TpcdSupportManager::TpcdSupportManager(
    content::WebContents* web_contents,
    std::unique_ptr<TpcdSupportDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TpcdSupportManager>(*web_contents),
      delegate_(std::move(delegate)){};

TpcdSupportManager::~TpcdSupportManager() = default;

void TpcdSupportDelegate::Update3pcdSupportSettings(
    const url::Origin& request_origin,
    const url::Origin& partition_origin,
    bool enrolled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  const GURL request_site_as_url = net::SchemefulSite(request_origin).GetURL();
  const GURL partition_site_as_url =
      net::SchemefulSite(partition_origin).GetURL();

  // Check for an existing enrollment setting for the pair.
  content_settings::SettingInfo info;
  bool existing_setting = (settings_map->GetContentSetting(
                               request_site_as_url, partition_site_as_url,
                               ContentSettingsType::TPCD_SUPPORT,
                               &info) == CONTENT_SETTING_ALLOW) &&
                          !info.primary_pattern.MatchesAllHosts() &&
                          !info.secondary_pattern.MatchesAllHosts();
  // If the enrollment status matches existing settings, there is no need to
  // update |settings_map|.
  if (enrolled == existing_setting) {
    return;
  }

  if (enrolled) {
    settings_map->SetContentSettingDefaultScope(
        request_site_as_url, partition_site_as_url,
        ContentSettingsType::TPCD_SUPPORT, CONTENT_SETTING_ALLOW);
  } else {
    ContentSettingsPattern primary_site_pattern =
        ContentSettingsPattern::CreateBuilder()
            ->WithScheme(request_site_as_url.scheme())
            ->WithDomainWildcard()
            ->WithHost(request_site_as_url.host())
            ->WithPathWildcard()
            ->WithPortWildcard()
            ->Build();
    ContentSettingsPattern secondary_site_pattern =
        ContentSettingsPattern::CreateBuilder()
            ->WithScheme(partition_site_as_url.scheme())
            ->WithDomainWildcard()
            ->WithHost(partition_site_as_url.host())
            ->WithPathWildcard()
            ->WithPortWildcard()
            ->Build();

    // Remove settings for expired/unused pairs to avoid memory bloat.
    auto matches_pair =
        [&](const ContentSettingPatternSource& setting) -> bool {
      return (setting.primary_pattern == primary_site_pattern) &&
             (setting.secondary_pattern == secondary_site_pattern);
    };

    settings_map->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::TPCD_SUPPORT, matches_pair);
  }

  ContentSettingsForOneType enrollments =
      settings_map->GetSettingsForOneType(ContentSettingsType::TPCD_SUPPORT);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettingsFor3pcd(enrollments);
}

// Persistent Origin Trials can only be checked on the UI thread.
void TpcdSupportManager::Check3pcdTrialOnUiThread(
    ContentSettingUpdateCallback done_callback,
    const url::Origin& request_origin,
    const url::Origin& partition_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::OriginTrialsControllerDelegate* trial_delegate =
      WebContentsObserver::web_contents()
          ->GetBrowserContext()
          ->GetOriginTrialsControllerDelegate();
  if (!trial_delegate) {
    return;
  }

  bool enrolled = trial_delegate->IsFeaturePersistedForOrigin(
      request_origin, partition_origin, blink::OriginTrialFeature::kTpcd,
      base::Time::Now());

  std::move(done_callback)
      .Run(std::move(request_origin), std::move(partition_origin), enrolled);
}

void TpcdSupportManager::Check3pcdTrialAsync(
    ContentSettingUpdateCallback done_callback,
    const url::Origin& request_origin,
    const url::Origin& partition_origin) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TpcdSupportManager::Check3pcdTrialOnUiThread,
                     weak_factory_.GetWeakPtr(), std::move(done_callback),
                     std::move(request_origin), std::move(partition_origin)));
}

void TpcdSupportManager::Update3pcdSupportSettings(
    const url::Origin& request_origin,
    const url::Origin& partition_origin,
    bool enrolled) {
  delegate_->Update3pcdSupportSettings(request_origin, partition_origin,
                                       enrolled);
}

void TpcdSupportManager::OnNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  // Navigations in the outermost main frame should not be considered.
  if (navigation_handle->IsInOutermostMainFrame()) {
    return;
  }

  url::Origin request_origin = url::Origin::Create(navigation_handle->GetURL());
  url::Origin partition_origin = WebContentsObserver::web_contents()
                                     ->GetPrimaryMainFrame()
                                     ->GetLastCommittedOrigin();

  if (request_origin.opaque() || partition_origin.opaque()) {
    return;
  }

  Check3pcdTrialAsync(
      base::BindOnce(&TpcdSupportManager::Update3pcdSupportSettings,
                     weak_factory_.GetWeakPtr()),
      std::move(request_origin), std::move(partition_origin));
}

void TpcdSupportManager::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  OnNavigationResponse(navigation_handle);
}

void TpcdSupportManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  OnNavigationResponse(navigation_handle);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TpcdSupportManager);
