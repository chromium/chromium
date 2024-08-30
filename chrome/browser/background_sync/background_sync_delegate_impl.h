// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "components/background_sync/background_sync_delegate.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

class Profile;
class HostContentSettingsMap;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace ukm {
class UkmBackgroundRecorderService;
}  // namespace ukm

// Chrome's customization of the logic in components/background_sync
class BackgroundSyncDelegateImpl
    : public background_sync::BackgroundSyncDelegate,
      public site_engagement::SiteEngagementObserver {
 public:
  static const int kEngagementLevelNonePenalty = 0;
  static const int kEngagementLevelHighOrMaxPenalty = 1;
  static const int kEngagementLevelLowOrMediumPenalty = 2;
  static const int kEngagementLevelMinimalPenalty = 3;

  explicit BackgroundSyncDelegateImpl(Profile* profile);
  ~BackgroundSyncDelegateImpl() override;

#if !BUILDFLAG(IS_ANDROID)
  class BackgroundSyncEventKeepAliveImpl
      : public content::BackgroundSyncController::BackgroundSyncEventKeepAlive {
   public:
    ~BackgroundSyncEventKeepAliveImpl() override;
    explicit BackgroundSyncEventKeepAliveImpl(Profile* profile);

   private:
    std::unique_ptr<ScopedKeepAlive, content::BrowserThread::DeleteOnUIThread>
        keepalive_;
    std::unique_ptr<ScopedProfileKeepAlive,
                    content::BrowserThread::DeleteOnUIThread>
        profile_keepalive_;
  };

  std::unique_ptr<
      content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() override;
#endif

  void GetUkmSourceId(
      const url::Origin& origin,
      base::OnceCallback<void(std::optional<ukm::SourceId>)> callback) override;
  void Shutdown() override;
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  bool IsProfileOffTheRecord() override;
  void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) override;
  int GetSiteEngagementPenalty(const GURL& url) override;
#if BUILDFLAG(IS_ANDROID)
  void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) override;
  void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type) override;
  bool ShouldDisableBackgroundSync() override;
  bool ShouldDisableAndroidNetworkDetection() override;
#endif  // BUILDFLAG(IS_ANDROID)

  // SiteEngagementObserver overrides.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         double old_score,
                         site_engagement::EngagementType engagement_type,
                         const std::optional<webapps::AppId>& app_id) override;

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;
  bool off_the_record_;
  raw_ptr<ukm::UkmBackgroundRecorderService, DanglingUntriaged>
      ukm_background_service_;
  // Same lifetime as |profile_|.
  raw_ptr<site_engagement::SiteEngagementService, DanglingUntriaged>
      site_engagement_service_;
  std::set<url::Origin> suspended_periodic_sync_origins_;
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_
