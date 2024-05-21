// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using Source = visited_url_ranking::URLVisit::Source;

namespace {
std::u16string FormatRelativeTime(const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

// Helper method to create mojom tab objects from SessionTab objects.
history::mojom::TabPtr SessionTabToMojom(const std::string& session_name) {
  auto tab_mojom = history::mojom::Tab::New();
  tab_mojom->device_type = history::mojom::DeviceType(0);
  tab_mojom->session_name = session_name;
  tab_mojom->url = GURL("https://www.google.com");
  tab_mojom->title = "Sample Title";

  tab_mojom->decorator = history::mojom::Decorator(0);
  base::TimeDelta relative_time = base::Minutes(5);
  tab_mojom->relative_time = relative_time;
  if (relative_time.InSeconds() < 60) {
    tab_mojom->relative_time_text = l10n_util::GetStringUTF8(
        IDS_NTP_MODULES_TAB_RESUMPTION_RECENTLY_OPENED);
  } else {
    tab_mojom->relative_time_text = base::UTF16ToUTF8(
        FormatRelativeTime(base::Time::Now() - base::Minutes(5)));
  }

  return tab_mojom;
}
}  // namespace

MostRelevantTabResumptionPageHandler::MostRelevantTabResumptionPageHandler(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

MostRelevantTabResumptionPageHandler::~MostRelevantTabResumptionPageHandler() =
    default;

void MostRelevantTabResumptionPageHandler::GetTabs(GetTabsCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleDataParam);

  if (!fake_data_param.empty()) {
    std::vector<history::mojom::TabPtr> tabs_mojom;
    const int kSampleVisitsCount = 3;
    for (int i = 0; i < kSampleVisitsCount; i++) {
      tabs_mojom.push_back(SessionTabToMojom("Test Session"));
    }
    std::move(callback).Run(std::move(tabs_mojom));
    return;
  }

  auto fetch_options =
      FetchOptions::CreateDefaultFetchOptionsForTabResumption();
  // Filter certain content categories, generally for use cases where a device
  // and profile may be shared by multiple family members.
  fetch_options.transforms.insert(
      fetch_options.transforms.begin(),
      URLVisitAggregatesTransformType::kHistoryCategoriesFilter);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  // TODO (crbug.com/329243396): Wire call to `RankURLVisitAggregates`.
  visited_url_ranking_service->FetchURLVisitAggregates(
      fetch_options,
      base::BindOnce(
          &MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates(
    GetTabsCallback callback,
    visited_url_ranking::ResultStatus status,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  std::vector<history::mojom::TabPtr> tabs_mojom;
  for (const auto& url_visit_aggregate : url_visit_aggregates) {
    auto tab_mojom = history::mojom::Tab::New();
    // TODO(crbug.com/338622450): Wire fields to be displayed on the UI.
    tab_mojom->url = **url_visit_aggregate.GetAssociatedURLs().begin();
    tabs_mojom.push_back(std::move(tab_mojom));
  }

  std::move(callback).Run(std::move(tabs_mojom));
}
