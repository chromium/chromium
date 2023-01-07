// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"

namespace {

std::vector<std::string> GetEngagedSitesInBackground(
    base::Time now,
    scoped_refptr<HostContentSettingsMap> map) {
  std::set<std::string> unique_sites;
  auto details =
      site_engagement::SiteEngagementService::GetAllDetailsInBackground(now,
                                                                        map);
  for (const site_engagement::mojom::SiteEngagementDetails& detail : details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    if (!site_engagement::SiteEngagementService::IsEngagementAtLeast(
            detail.total_score, blink::mojom::EngagementLevel::MINIMAL)) {
      continue;
    }
    unique_sites.insert(GetSiteForDIPS(detail.origin));
  }

  return std::vector(unique_sites.begin(), unique_sites.end());
}

}  // namespace

DIPSService::DIPSService(content::BrowserContext* context)
    : browser_context_(context),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context))),
      storage_(base::SequenceBound<DIPSStorage>(CreateTaskRunner())) {
  // TODO(crbug.com/1342228): Persist DB to disk for non-OTR profiles.
  storage_.AsyncCall(&DIPSStorage::Init).WithArgs(absl::nullopt);
  // TODO: Prevent use of the DB until prepopulation starts.
  InitializeStorageWithEngagedSites();
}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

void DIPSService::Shutdown() {
  cookie_settings_.reset();
}

scoped_refptr<base::SequencedTaskRunner> DIPSService::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

bool DIPSService::ShouldBlockThirdPartyCookies() const {
  return cookie_settings_->ShouldBlockThirdPartyCookies();
}

void DIPSService::InitializeStorageWithEngagedSites() {
  base::Time now = base::Time::Now();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &GetEngagedSitesInBackground, now,
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(browser_context_))),
      base::BindOnce(&DIPSService::InitializeStorage,
                     weak_factory_.GetWeakPtr(), now));
}

void DIPSService::InitializeStorage(base::Time time,
                                    std::vector<std::string> sites) {
  storage_.AsyncCall(&DIPSStorage::Prepopulate).WithArgs(time, sites);
}
