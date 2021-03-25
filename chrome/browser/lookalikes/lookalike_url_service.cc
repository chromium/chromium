// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace {

constexpr base::TimeDelta kEngagedSiteUpdateInterval =
    base::TimeDelta::FromSeconds(60);

class LookalikeUrlServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LookalikeUrlService* GetForProfile(Profile* profile) {
    return static_cast<LookalikeUrlService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static LookalikeUrlServiceFactory* GetInstance() {
    return base::Singleton<LookalikeUrlServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<LookalikeUrlServiceFactory>;

  // LookalikeUrlServiceFactory();
  LookalikeUrlServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "LookalikeUrlServiceFactory",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
  }

  ~LookalikeUrlServiceFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new LookalikeUrlService(static_cast<Profile*>(profile));
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlServiceFactory);
};

// static
std::vector<DomainInfo> UpdateEngagedSitesOnWorkerThread(
    base::Time now,
    scoped_refptr<HostContentSettingsMap> map) {
  TRACE_EVENT0("navigation",
               "LookalikeUrlService UpdateEngagedSitesOnWorkerThread");
  std::vector<DomainInfo> new_engaged_sites;

  auto details =
      site_engagement::SiteEngagementService::GetAllDetailsInBackground(now,
                                                                        map);
  TRACE_EVENT1("navigation", "LookalikeUrlService SiteEngagementService",
               "site_count", details.size());
  for (const site_engagement::mojom::SiteEngagementDetails& detail : details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    // Ignore sites with an engagement score below threshold.
    if (detail.total_score <
        site_engagement::SiteEngagementScore::GetMediumEngagementBoundary()) {
      continue;
    }
    const DomainInfo domain_info = GetDomainInfo(detail.origin);
    if (domain_info.domain_and_registry.empty()) {
      continue;
    }
    new_engaged_sites.push_back(domain_info);
  }

  return new_engaged_sites;
}

}  // namespace

// static
const base::FeatureParam<base::TimeDelta>
    LookalikeUrlService::kManifestFetchDelay{
        &lookalikes::features::kLookalikeDigitalAssetLinks,
        lookalikes::features::kLookalikeDigitalAssetLinksTimeoutParameter,
        base::TimeDelta::FromSeconds(5)};

LookalikeUrlService::LookalikeUrlService(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}

LookalikeUrlService::~LookalikeUrlService() = default;

// static
LookalikeUrlService* LookalikeUrlService::Get(Profile* profile) {
  return LookalikeUrlServiceFactory::GetForProfile(profile);
}

bool LookalikeUrlService::EngagedSitesNeedUpdating() const {
  if (last_engagement_fetch_time_.is_null())
    return true;
  const base::TimeDelta elapsed = clock_->Now() - last_engagement_fetch_time_;
  return elapsed >= kEngagedSiteUpdateInterval;
}

void LookalikeUrlService::ForceUpdateEngagedSites(
    EngagedSitesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("navigation", "LookalikeUrlService::ForceUpdateEngagedSites");

  // Queue an update on a worker thread if necessary.
  if (!update_in_progress_) {
    update_in_progress_ = true;

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            &UpdateEngagedSitesOnWorkerThread,
            clock_->Now(),
            base::WrapRefCounted(
                HostContentSettingsMapFactory::GetForProfile(profile_))),
        base::BindOnce(&LookalikeUrlService::OnUpdateEngagedSitesCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  // Postpone the execution of the callback after the update is completed.
  pending_update_complete_callbacks_.push_back(std::move(callback));
}

const std::vector<DomainInfo> LookalikeUrlService::GetLatestEngagedSites()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return engaged_sites_;
}

void LookalikeUrlService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void LookalikeUrlService::OnUpdateEngagedSitesCompleted(
    std::vector<DomainInfo> new_engaged_sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(update_in_progress_);
  TRACE_EVENT0("navigation",
               "LookalikeUrlService::OnUpdateEngagedSitesCompleted");
  engaged_sites_.swap(new_engaged_sites);
  last_engagement_fetch_time_ = clock_->Now();
  update_in_progress_ = false;

  // Call pending callbacks.
  std::vector<EngagedSitesCallback> callbacks;
  callbacks.swap(pending_update_complete_callbacks_);
  for (auto&& callback : callbacks)
    std::move(callback).Run(engaged_sites_);
}
