// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/history_aware_site_engagement_service.h"

#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "content/public/browser/browser_context.h"

namespace site_engagement {

HistoryAwareSiteEngagementService::HistoryAwareSiteEngagementService(
    content::BrowserContext* browser_context,
    history::HistoryService* history_service)
    : SiteEngagementService(browser_context) {
  // May be null in tests.
  if (history_service)
    history_service_observation_.Observe(history_service);
}

HistoryAwareSiteEngagementService::~HistoryAwareSiteEngagementService() =
    default;

void HistoryAwareSiteEngagementService::Shutdown() {
  history_service_observation_.Reset();
}

void HistoryAwareSiteEngagementService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  std::multiset<GURL> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows())
    origins.insert(row.url().DeprecatedGetOriginAsURL());

  UpdateEngagementScores(origins, deletion_info.is_from_expiration(),
                         deletion_info.deleted_urls_origin_map());
}

void HistoryAwareSiteEngagementService::UpdateEngagementScores(
    const std::multiset<GURL>& deleted_origins,
    bool expired,
    const history::OriginCountAndLastVisitMap& remaining_origins) {
  // The most in-the-past option in the Clear Browsing Dialog aside from "all
  // time" is 4 weeks ago. Set the last updated date to 4 weeks ago for origins
  // where we can't find a valid last visit date.
  base::Time now = clock().Now();
  base::Time four_weeks_ago = now - base::Days(28);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());

  for (const auto& origin_to_count : remaining_origins) {
    GURL origin = origin_to_count.first;
    // It appears that the history service occasionally sends bad URLs to us.
    // See crbug.com/612881.
    if (!origin.is_valid())
      continue;

    int remaining = origin_to_count.second.first;
    base::Time last_visit = origin_to_count.second.second;
    int deleted = deleted_origins.count(origin);

    // Do not update engagement scores if the deletion was an expiry, but the
    // URL still has entries in history.
    if ((expired && remaining != 0) || deleted == 0)
      continue;

    // Remove origins that have no urls left.
    if (remaining == 0) {
      settings_map->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::SITE_ENGAGEMENT, base::Value());
      continue;
    }

    // Remove engagement proportional to the urls expired from the origin's
    // entire history.
    double proportion_remaining =
        static_cast<double>(remaining) / (remaining + deleted);
    if (last_visit.is_null() || last_visit > now)
      last_visit = four_weeks_ago;

    // At this point, we are going to proportionally decay the origin's
    // engagement, and reset its last visit date to the last visit to a URL
    // under the origin in history. If this new last visit date is long enough
    // in the past, the next time the origin's engagement is accessed the
    // automatic decay will kick in - i.e. a double decay will have occurred.
    // To prevent this, compute the decay that would have taken place since the
    // new last visit and add it to the engagement at this point. When the
    // engagement is next accessed, it will decay back to the proportionally
    // reduced value rather than being decayed once here, and then once again
    // when it is next accessed.
    // TODO(crbug.com/41308686): Move the proportional decay logic into
    // SiteEngagementScore, so it can decay raw_score_ directly, without the
    // double-decay issue.
    SiteEngagementScore engagement_score = CreateEngagementScore(origin);

    double new_score = proportion_remaining * engagement_score.GetTotalScore();
    int hours_since_engagement = (now - last_visit).InHours();
    int periods =
        hours_since_engagement / SiteEngagementScore::GetDecayPeriodInHours();
    new_score += periods * SiteEngagementScore::GetDecayPoints();
    new_score *= pow(1.0 / SiteEngagementScore::GetDecayProportion(), periods);

    double score = std::min(SiteEngagementScore::kMaxPoints, new_score);
    engagement_score.Reset(score, last_visit);
    if (!engagement_score.last_shortcut_launch_time().is_null() &&
        engagement_score.last_shortcut_launch_time() > last_visit) {
      engagement_score.set_last_shortcut_launch_time(last_visit);
    }

    engagement_score.Commit();
  }

  SetLastEngagementTime(now);
}

}  // namespace site_engagement
