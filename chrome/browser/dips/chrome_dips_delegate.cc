// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/chrome_dips_delegate.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/browser_context.h"

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

// static
std::unique_ptr<content::DipsDelegate> ChromeDipsDelegate::Create() {
  return std::make_unique<ChromeDipsDelegate>();
}

void ChromeDipsDelegate::GetEngagedSites(
    content::BrowserContext* browser_context,
    EngagedSitesCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &GetEngagedSitesInBackground, base::Time::Now(),
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(browser_context))),
      std::move(callback));
}
