// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_delegate_impl.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
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

#if !defined(OS_ANDROID)
BackgroundSyncDelegateImpl::BackgroundSyncEventKeepAliveImpl::
    BackgroundSyncEventKeepAliveImpl(Profile* profile) {
  keepalive_ = std::unique_ptr<ScopedKeepAlive,
                               content::BrowserThread::DeleteOnUIThread>(
      new ScopedKeepAlive(KeepAliveOrigin::BACKGROUND_SYNC,
                          KeepAliveRestartOption::DISABLED));
  profile_keepalive_ =
      std::unique_ptr<ScopedProfileKeepAlive,
                      content::BrowserThread::DeleteOnUIThread>(
          new ScopedProfileKeepAlive(profile,
                                     ProfileKeepAliveOrigin::kBackgroundSync));
}

BackgroundSyncDelegateImpl::BackgroundSyncEventKeepAliveImpl::
    ~BackgroundSyncEventKeepAliveImpl() = default;

std::unique_ptr<content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
BackgroundSyncDelegateImpl::CreateBackgroundSyncEventKeepAlive() {
  if (!KeepAliveRegistry::GetInstance()->IsShuttingDown())
    return std::make_unique<BackgroundSyncEventKeepAliveImpl>(profile_);
  return nullptr;
}
#endif  // !defined(OS_ANDROID)

void BackgroundSyncDelegateImpl::GetUkmSourceId(
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<ukm::SourceId>)> callback) {
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
        url::Origin::Create(url.GetOrigin()));
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

  NOTREACHED();
  return kEngagementLevelNonePenalty;
}

#if defined(OS_ANDROID)

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

#endif  // defined(OS_ANDROID)

void BackgroundSyncDelegateImpl::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    site_engagement::EngagementType engagement_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (score == 0.0)
    return;

  auto origin = url::Origin::Create(url);
  auto iter = suspended_periodic_sync_origins_.find(origin);
  if (iter == suspended_periodic_sync_origins_.end())
    return;

  suspended_periodic_sync_origins_.erase(iter);

  auto* storage_partition = content::BrowserContext::GetStoragePartitionForUrl(
      profile_, url, /* can_create= */ false);
  if (!storage_partition)
    return;

  auto* background_sync_context = storage_partition->GetBackgroundSyncContext();
  if (!background_sync_context)
    return;

  background_sync_context->RevivePeriodicBackgroundSyncRegistrations(
      std::move(origin));
}
