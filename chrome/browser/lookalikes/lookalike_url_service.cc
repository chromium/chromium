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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace {

constexpr uint32_t kEngagedSiteUpdateIntervalInSeconds = 5 * 60;

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
    DependsOn(SiteEngagementServiceFactory::GetInstance());
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

std::string GetETLDPlusOne(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

DomainInfo::DomainInfo(const std::string& arg_domain_and_registry,
                       const std::string& arg_domain_without_registry,
                       const url_formatter::IDNConversionResult& arg_idn_result,
                       const url_formatter::Skeletons& arg_skeletons)
    : domain_and_registry(arg_domain_and_registry),
      domain_without_registry(arg_domain_without_registry),
      idn_result(arg_idn_result),
      skeletons(arg_skeletons) {}

DomainInfo::~DomainInfo() = default;

DomainInfo::DomainInfo(const DomainInfo&) = default;

DomainInfo GetDomainInfo(const GURL& url) {
  if (net::IsLocalhost(url) || net::IsHostnameNonUnique(url.host())) {
    return DomainInfo(std::string(), std::string(),
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  // Perform all computations on eTLD+1.
  const std::string domain_and_registry = GetETLDPlusOne(url.host());
  const std::string domain_without_registry =
      domain_and_registry.empty()
          ? std::string()
          : url_formatter::top_domains::HostnameWithoutRegistry(
                domain_and_registry);

  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty()) {
    return DomainInfo(domain_and_registry, domain_without_registry,
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  // Compute skeletons using eTLD+1, skipping all spoofing checks. Spoofing
  // checks in url_formatter can cause the converted result to be punycode.
  // We want to avoid this in order to get an accurate skeleton for the unicode
  // version of the domain.
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_and_registry);
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(idn_result.result);
  return DomainInfo(domain_and_registry, domain_without_registry, idn_result,
                    skeletons);
}

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
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &SiteEngagementService::GetAllDetailsInBackground, clock_->Now(),
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
    std::vector<mojom::SiteEngagementDetails> details) {
  SiteEngagementService* service = SiteEngagementService::Get(profile_);
  engaged_sites_.clear();
  for (const mojom::SiteEngagementDetails& detail : details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    // Ignore sites with an engagement score below threshold.
    if (!service->IsEngagementAtLeast(detail.origin,
                                      blink::mojom::EngagementLevel::MEDIUM)) {
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
