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
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace {

constexpr uint32_t kEngagedSiteUpdateIntervalInSeconds = 60;

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

}  // namespace

LookalikeUrlService::LookalikeUrlService(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}

LookalikeUrlService::~LookalikeUrlService() {}

// static
LookalikeUrlService* LookalikeUrlService::Get(Profile* profile) {
  return LookalikeUrlServiceFactory::GetForProfile(profile);
}

bool LookalikeUrlService::EngagedSitesNeedUpdating() {
  if (!last_engagement_fetch_time_.is_null()) {
    const base::TimeDelta elapsed = clock_->Now() - last_engagement_fetch_time_;
    if (elapsed <
        base::TimeDelta::FromSeconds(kEngagedSiteUpdateIntervalInSeconds)) {
      return false;
    }
  }
  return true;
}

void LookalikeUrlService::ForceUpdateEngagedSites(
    EngagedSitesCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &site_engagement::SiteEngagementService::GetAllDetailsInBackground,
          clock_->Now(),
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(profile_))),
      base::BindOnce(&LookalikeUrlService::OnFetchEngagedSites,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

const std::vector<DomainInfo> LookalikeUrlService::GetLatestEngagedSites()
    const {
  return engaged_sites_;
}

void LookalikeUrlService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void LookalikeUrlService::OnFetchEngagedSites(
    EngagedSitesCallback callback,
    std::vector<site_engagement::mojom::SiteEngagementDetails> details) {
  engaged_sites_.clear();
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
    engaged_sites_.push_back(domain_info);
  }
  last_engagement_fetch_time_ = clock_->Now();
  std::move(callback).Run(engaged_sites_);
}
