// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_

#include "content/public/browser/background_sync_controller.h"

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/background_sync/background_sync_metrics.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/gurl.h"

namespace content {
struct BackgroundSyncParameters;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class Profile;
class SiteEngagementService;
class GURL;

class BackgroundSyncControllerImpl : public content::BackgroundSyncController,
                                     public SiteEngagementObserver,
                                     public KeyedService {
 public:
  static const char kFieldTrialName[];
  static const char kDisabledParameterName[];
  static const char kKeepBrowserAwakeParameterName[];
  static const char kMaxAttemptsParameterName[];
  static const char kRelyOnAndroidNetworkDetection[];
  static const char kMaxAttemptsWithNotificationPermissionParameterName[];
  static const char kInitialRetryParameterName[];
  static const char kRetryDelayFactorParameterName[];
  static const char kMinSyncRecoveryTimeName[];
  static const char kMaxSyncEventDurationName[];
  static const char kMinPeriodicSyncEventsInterval[];

  static const int kEngagementLevelNonePenalty = 0;
  static const int kEngagementLevelHighOrMaxPenalty = 1;
  static const int kEngagementLevelLowOrMediumPenalty = 2;
  static const int kEngagementLevelMinimalPenalty = 3;

#if !defined(OS_ANDROID)
  class BackgroundSyncEventKeepAliveImpl : public BackgroundSyncEventKeepAlive {
   public:
    ~BackgroundSyncEventKeepAliveImpl() override;
    BackgroundSyncEventKeepAliveImpl();

   private:
    std::unique_ptr<ScopedKeepAlive, content::BrowserThread::DeleteOnUIThread>
        keepalive_ = nullptr;
  };
#endif

  explicit BackgroundSyncControllerImpl(Profile* profile);
  ~BackgroundSyncControllerImpl() override;

  // content::BackgroundSyncController overrides.
  void GetParameterOverrides(
      content::BackgroundSyncParameters* parameters) override;
  void NotifyOneShotBackgroundSyncRegistered(const url::Origin& origin,
                                             bool can_fire,
                                             bool is_reregistered) override;
  void NotifyPeriodicBackgroundSyncRegistered(const url::Origin& origin,
                                              int min_interval,
                                              bool is_reregistered) override;
  void NotifyOneShotBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) override;
  void NotifyPeriodicBackgroundSyncCompleted(
      const url::Origin& origin,
      blink::ServiceWorkerStatusCode status_code,
      int num_attempts,
      int max_attempts) override;
  void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) override;
  void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type) override;

  base::TimeDelta GetNextEventDelay(
      const content::BackgroundSyncRegistration& registration,
      content::BackgroundSyncParameters* parameters,
      base::TimeDelta time_till_soonest_scheduled_event_for_origin) override;

  std::unique_ptr<BackgroundSyncEventKeepAlive>
  CreateBackgroundSyncEventKeepAlive() override;
  void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) override;

  // SiteEngagementObserver overrides.
  void OnEngagementEvent(
      content::WebContents* web_contents,
      const GURL& url,
      double score,
      SiteEngagementService::EngagementType engagement_type) override;

 private:
  // Gets the site engagement penalty for |url|, which is inversely proportional
  // to the engagement level. The lower the engagement levels with the site,
  // the less often periodic sync events will be fired.
  // Returns kEngagementLevelNonePenalty if the engagement level is
  // blink::mojom::EngagementLevel::NONE.
  int GetSiteEngagementPenalty(const GURL& url);

  // Once we've identified the minimum number of hours between each periodicsync
  // event for an origin, every delay calculated for the origin should be a
  // multiple of the same.
  base::TimeDelta SnapToMaxOriginFrequency(int64_t min_interval,
                                           int64_t min_gap_for_origin);

  // Returns an updated delay for a Periodic Background Sync registration -- one
  // that ensures the |min_gap_for_origin|.
  base::TimeDelta ApplyMinGapForOrigin(
      base::TimeDelta delay,
      base::TimeDelta time_till_next_scheduled_event_for_origin,
      base::TimeDelta min_gap_for_origin);

  Profile* profile_;  // This object is owned by profile_.

  // Same lifetime as |profile_|.
  SiteEngagementService* site_engagement_service_;

  BackgroundSyncMetrics background_sync_metrics_;

  std::set<url::Origin> suspended_periodic_sync_origins_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncControllerImpl);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_IMPL_H_
