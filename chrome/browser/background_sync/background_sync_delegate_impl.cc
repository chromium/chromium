// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_delegate_impl.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

BackgroundSyncDelegateImpl::BackgroundSyncDelegateImpl(Profile* profile)
    : SiteEngagementObserver(
          site_engagement::SiteEngagementService::Get(profile)),
      profile_(profile),
      ukm_background_service_(
          ukm::UkmBackgroundRecorderFactory::GetForProfile(profile)),

      site_engagement_service_(
          site_engagement::SiteEngagementService::Get(profile)) {
  DCHECK(profile_);
  DCHECK(ukm_background_service_);
  DCHECK(site_engagement_service_);
  off_the_record_ = profile_->IsOffTheRecord();
}

BackgroundSyncDelegateImpl::~BackgroundSyncDelegateImpl() = default;

#if !BUILDFLAG(IS_ANDROID)
BackgroundSyncDelegateImpl::BackgroundSyncEventKeepAliveImpl::
    BackgroundSyncEventKeepAliveImpl(Profile* profile) {
  keepalive_ = std::unique_ptr<ScopedKeepAlive,
                               content::BrowserThread::DeleteOnUIThread>(
      new ScopedKeepAlive(KeepAliveOrigin::BACKGROUND_SYNC,
                          KeepAliveRestartOption::DISABLED));
  if (!profile->IsOffTheRecord()) {
    // TODO(crbug.com/40159237): Remove this guard when OTR profiles become
    // refcounted and support ScopedProfileKeepAlive.
    profile_keepalive_ =
        std::unique_ptr<ScopedProfileKeepAlive,
                        content::BrowserThread::DeleteOnUIThread>(
            new ScopedProfileKeepAlive(
                profile, ProfileKeepAliveOrigin::kBackgroundSync));
  }
}

BackgroundSyncDelegateImpl::BackgroundSyncEventKeepAliveImpl::
    ~BackgroundSyncEventKeepAliveImpl() = default;

std::unique_ptr<content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
BackgroundSyncDelegateImpl::CreateBackgroundSyncEventKeepAlive() {
  if (!KeepAliveRegistry::GetInstance()->IsShuttingDown())
    return std::make_unique<BackgroundSyncEventKeepAliveImpl>(profile_);
  return nullptr;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void BackgroundSyncDelegateImpl::GetUkmSourceId(
    const url::Origin& origin,
    base::OnceCallback<void(std::optional<ukm::SourceId>)> callback) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(origin,
                                                          std::move(callback));
}

void BackgroundSyncDelegateImpl::Shutdown() {
  // Clear the profile as we're not supposed to use it anymore.
  profile_ = nullptr;
}

HostContentSettingsMap*
BackgroundSyncDelegateImpl::GetHostContentSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(profile_);
}

bool BackgroundSyncDelegateImpl::IsProfileOffTheRecord() {
  return off_the_record_;
}

void BackgroundSyncDelegateImpl::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto& origin : suspended_origins)
    suspended_periodic_sync_origins_.insert(std::move(origin));
}

int BackgroundSyncDelegateImpl::GetSiteEngagementPenalty(const GURL& url) {
  blink::mojom::EngagementLevel engagement_level =
      site_engagement_service_->GetEngagementLevel(url);
  if (engagement_level == blink::mojom::EngagementLevel::NONE) {
    suspended_periodic_sync_origins_.insert(
        url::Origin::Create(url.DeprecatedGetOriginAsURL()));
  }

  switch (engagement_level) {
    case blink::mojom::EngagementLevel::NONE:
      // Suspend registration until site_engagement improves.
      return kEngagementLevelNonePenalty;
    case blink::mojom::EngagementLevel::MINIMAL:
      return kEngagementLevelMinimalPenalty;
    case blink::mojom::EngagementLevel::LOW:
    case blink::mojom::EngagementLevel::MEDIUM:
      return kEngagementLevelLowOrMediumPenalty;
    case blink::mojom::EngagementLevel::HIGH:
    case blink::mojom::EngagementLevel::MAX:
      // Very few sites reach max engagement level.
      return kEngagementLevelHighOrMaxPenalty;
  }

  NOTREACHED_IN_MIGRATION();
  return kEngagementLevelNonePenalty;
}

#if BUILDFLAG(IS_ANDROID)

void BackgroundSyncDelegateImpl::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  BackgroundSyncLauncherAndroid::ScheduleBrowserWakeUpWithDelay(sync_type,
                                                                delay);
}

void BackgroundSyncDelegateImpl::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  BackgroundSyncLauncherAndroid::CancelBrowserWakeup(sync_type);
}

bool BackgroundSyncDelegateImpl::ShouldDisableBackgroundSync() {
  return BackgroundSyncLauncherAndroid::ShouldDisableBackgroundSync();
}

bool BackgroundSyncDelegateImpl::ShouldDisableAndroidNetworkDetection() {
  return false;
}

#endif  // BUILDFLAG(IS_ANDROID)

void BackgroundSyncDelegateImpl::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    double old_score,
    site_engagement::EngagementType engagement_type,
    const std::optional<webapps::AppId>& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (score == 0.0)
    return;

  auto origin = url::Origin::Create(url);
  auto iter = suspended_periodic_sync_origins_.find(origin);
  if (iter == suspended_periodic_sync_origins_.end())
    return;

  suspended_periodic_sync_origins_.erase(iter);

  // Engagement is always accumulated in the main frame.
  auto* storage_partition =
      web_contents->GetPrimaryMainFrame()->GetStoragePartition();
  if (!storage_partition)
    return;

  auto* background_sync_context = storage_partition->GetBackgroundSyncContext();
  if (!background_sync_context)
    return;

  background_sync_context->RevivePeriodicBackgroundSyncRegistrations(
      std::move(origin));
}
