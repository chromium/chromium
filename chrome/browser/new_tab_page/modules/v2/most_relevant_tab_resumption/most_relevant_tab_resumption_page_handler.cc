// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::URLVisit;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using Source = visited_url_ranking::URLVisit::Source;

namespace {
// Name of preference to track list of dismissed tabs.
const char kDismissedTabsPrefName[] =
    "NewTabPage.MostRelevantTabResumption.DismissedTabs";

std::u16string FormatRelativeTime(const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

// Helper method to create mojom tab objects from Tab objects.
history::mojom::TabPtr TabToMojom(const URLVisitAggregate::Tab& tab,
                                  base::Time last_active = base::Time()) {
  auto tab_mojom = history::mojom::Tab::New();
  tab_mojom->device_type =
      history::mojom::DeviceType(static_cast<int>(tab.visit.device_type));
  tab_mojom->session_name = tab.session_name;

  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, tab.visit.title,
                                    tab.visit.url);
  tab_mojom->title = *dictionary.FindString("title");
  tab_mojom->decorator = history::mojom::Decorator(0);

  auto last_visited =
      last_active.is_null() ? tab.visit.last_modified : last_active;
  base::TimeDelta relative_time = base::Time::Now() - last_visited;
  tab_mojom->relative_time = relative_time;
  if (relative_time < base::Minutes(1)) {
    tab_mojom->relative_time_text = l10n_util::GetStringUTF8(
        IDS_NTP_MODULES_TAB_RESUMPTION_RECENTLY_OPENED);
  } else {
    tab_mojom->relative_time_text =
        base::UTF16ToUTF8(FormatRelativeTime(last_visited));
  }
  tab_mojom->timestamp = last_visited;

  return tab_mojom;
}

// Helper method to create mojom tab objects from HistoryEntry objects.
history::mojom::TabPtr HistoryEntryVisitToMojom(
    const history::AnnotatedVisit& visit) {
  auto tab_mojom = history::mojom::Tab::New();
  tab_mojom->device_type =
      history::mojom::DeviceType(history::mojom::DeviceType::kUnknown);

  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, visit.url_row.title(),
                                    visit.url_row.url());
  tab_mojom->title = *dictionary.FindString("title");

  tab_mojom->decorator = history::mojom::Decorator(0);
  base::TimeDelta relative_time =
      base::Time::Now() - visit.url_row.last_visit();
  tab_mojom->relative_time = relative_time;
  if (relative_time < base::Minutes(1)) {
    tab_mojom->relative_time_text = l10n_util::GetStringUTF8(
        IDS_NTP_MODULES_TAB_RESUMPTION_RECENTLY_OPENED);
  } else {
    tab_mojom->relative_time_text =
        base::UTF16ToUTF8(FormatRelativeTime(visit.url_row.last_visit()));
  }
  tab_mojom->timestamp = visit.url_row.last_visit();

  return tab_mojom;
}

URLVisitAggregate::Tab CreateSampleURLVisitAggregateTab(
    const GURL& url,
    base::Time time = base::Time::Now() - base::Minutes(5)) {
  const std::u16string kSampleTitle(u"sample_title");
  return URLVisitAggregate::Tab(
      1,
      URLVisit(url, kSampleTitle, time,
               syncer::DeviceInfo::FormFactor::kDesktop,
               URLVisit::Source::kLocal),
      "Sample Session Tag", "Test Session");
}

FetchOptions::URLTypeSet AsURLTypeSet(
    const std::vector<std::string>& url_type_entries) {
  FetchOptions::URLTypeSet result_url_types = {};
  for (const auto& url_type_entry : url_type_entries) {
    int url_type;
    if (base::StringToInt(url_type_entry, &url_type)) {
      result_url_types.Put(static_cast<FetchOptions::URLType>(url_type));
    }
  }

  return result_url_types;
}

// Return the desired fetch result types as specified via feature params or the
// defaults if not specified.
FetchOptions::URLTypeSet GetFetchResultURLTypes() {
  const std::string module_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleDataParam);
  if (!module_data_param.empty() && module_data_param != "Fake Data") {
    const std::vector<std::string> data_param_url_types =
        base::SplitString(module_data_param, ",:;", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    FetchOptions::URLTypeSet result_url_types =
        AsURLTypeSet(data_param_url_types);
    if (!result_url_types.empty()) {
      return result_url_types;
    }
  }

  auto url_type_entries = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpMostRelevantTabResumptionModule,
          ntp_features::kNtpTabResumptionModuleResultTypesParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (url_type_entries.empty()) {
    return {FetchOptions::URLType::kActiveRemoteTab,
            FetchOptions::URLType::kRemoteVisit};
  }

  return AsURLTypeSet(url_type_entries);
}
}  // namespace

MostRelevantTabResumptionPageHandler::MostRelevantTabResumptionPageHandler(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      result_url_types_(GetFetchResultURLTypes()),
      dismissal_duration_days_(base::GetFieldTrialParamByFeatureAsInt(
          ntp_features::kNtpMostRelevantTabResumptionModule,
          ntp_features::kNtpTabResumptionModuleDismissalDurationParam,
          90)),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

MostRelevantTabResumptionPageHandler::~MostRelevantTabResumptionPageHandler() {
  RemoveOldDismissedTabs();
}

void MostRelevantTabResumptionPageHandler::GetTabs(GetTabsCallback callback) {
  const std::string data_type_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleDataParam);

  if (data_type_param == "Fake Data") {
    std::vector<history::mojom::TabPtr> tabs_mojom;
    const int kSampleVisitsCount = 3;
    for (int i = 0; i < kSampleVisitsCount; i++) {
      auto tab_mojom = TabToMojom(
          CreateSampleURLVisitAggregateTab(GURL("https://www.google.com")));
      tab_mojom->url = GURL("https://www.google.com");
      tab_mojom->url_key = "https://www.google.com";
      tab_mojom->training_request_id = 0;
      tabs_mojom.push_back(std::move(tab_mojom));
    }
    std::move(callback).Run(std::move(tabs_mojom));
    return;
  }

  FetchOptions fetch_options =
      FetchOptions::CreateFetchOptionsForTabResumption(result_url_types_);
  // Filter certain content categories, generally for use cases where a device
  // and profile may be shared by multiple family members.
  fetch_options.transforms.insert(
      fetch_options.transforms.begin(),
      URLVisitAggregatesTransformType::kHistoryCategoriesFilter);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->FetchURLVisitAggregates(
      fetch_options,
      base::BindOnce(
          &MostRelevantTabResumptionPageHandler::OnURLVisitAggregatesFetched,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                           base::PersistentHash("tab_resumption"));
}

void MostRelevantTabResumptionPageHandler::DismissModule(
    const std::vector<history::mojom::TabPtr> tabs) {
  RemoveOldDismissedTabs();
  ScopedListPrefUpdate tab_list(profile_->GetPrefs(), kDismissedTabsPrefName);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  for (const auto& tab : tabs) {
    tab_list->Append(base::Value(
        tab->url_key + ' ' +
        base::NumberToString(
            tab->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds())));
    visited_url_ranking_service->RecordAction(
        visited_url_ranking::ScoredURLUserAction::kDismissed, tab->url_key,
        segmentation_platform::TrainingRequestId(tab->training_request_id));
  }
}

void MostRelevantTabResumptionPageHandler::DismissTab(
    const history::mojom::TabPtr tab) {
  RemoveOldDismissedTabs();
  ScopedListPrefUpdate tab_list(profile_->GetPrefs(), kDismissedTabsPrefName);
  tab_list->Append(base::Value(
      tab->url_key + ' ' +
      base::NumberToString(
          tab->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds())));
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->RecordAction(
      visited_url_ranking::ScoredURLUserAction::kDismissed, tab->url_key,
      segmentation_platform::TrainingRequestId(tab->training_request_id));
}

void MostRelevantTabResumptionPageHandler::RestoreModule(
    const std::vector<history::mojom::TabPtr> tabs) {
  ScopedListPrefUpdate tab_list(profile_->GetPrefs(), kDismissedTabsPrefName);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  for (const auto& tab : tabs) {
    tab_list->EraseValue(base::Value(
        tab->url_key + ' ' +
        base::NumberToString(
            tab->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds())));
    visited_url_ranking_service->RecordAction(
        visited_url_ranking::ScoredURLUserAction::kSeen, tab->url_key,
        segmentation_platform::TrainingRequestId(tab->training_request_id));
  }
}

void MostRelevantTabResumptionPageHandler::RestoreTab(
    history::mojom::TabPtr tab) {
  ScopedListPrefUpdate tab_list(profile_->GetPrefs(), kDismissedTabsPrefName);
  tab_list->EraseValue(base::Value(
      tab->url_key + ' ' +
      base::NumberToString(
          tab->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds())));
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->RecordAction(
      visited_url_ranking::ScoredURLUserAction::kSeen, tab->url_key,
      segmentation_platform::TrainingRequestId(tab->training_request_id));
}

void MostRelevantTabResumptionPageHandler::OnURLVisitAggregatesFetched(
    GetTabsCallback callback,
    visited_url_ranking::ResultStatus status,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  if (status == visited_url_ranking::ResultStatus::kError) {
    std::move(callback).Run({});
    return;
  }

  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->RankURLVisitAggregates(
      {.key = visited_url_ranking::kTabResumptionRankerKey},
      std::move(url_visit_aggregates),
      base::BindOnce(
          &MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates(
    GetTabsCallback callback,
    visited_url_ranking::ResultStatus status,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  base::UmaHistogramEnumeration("NewTabPage.TabResumption.ResultStatus",
                                status);
  if (status == visited_url_ranking::ResultStatus::kError) {
    std::move(callback).Run({});
    return;
  }

  std::vector<history::mojom::TabPtr> tabs_mojom;
  for (const auto& url_visit_aggregate : url_visit_aggregates) {
    const URLVisitAggregate::TabData* tab_data =
        GetTabDataIfExists(url_visit_aggregate);
    if (tab_data) {
      const URLVisitAggregate::Tab* tab = &tab_data->last_active_tab;
      auto tab_mojom = TabToMojom(*tab, tab_data->last_active);
      tab_mojom->url = **url_visit_aggregate.GetAssociatedURLs().begin();
      tab_mojom->url_key = url_visit_aggregate.url_key;
      tab_mojom->training_request_id =
          url_visit_aggregate.request_id.GetUnsafeValue();
      if (IsNewURL(tab_mojom)) {
        tabs_mojom.push_back(std::move(tab_mojom));
        base::UmaHistogramEnumeration(
            "NewTabPage.TabResumption.URLVisitAggregateDataTypeDisplayed",
            URLVisitAggregateDataType::kTab);
      }
    } else {
      const history::AnnotatedVisit* visit =
          visited_url_ranking::GetHistoryEntryVisitIfExists(
              url_visit_aggregate);
      if (visit) {
        auto history_tab_mojom = HistoryEntryVisitToMojom(*visit);
        history_tab_mojom->url =
            **url_visit_aggregate.GetAssociatedURLs().begin();
        history_tab_mojom->url_key = url_visit_aggregate.url_key;
        history_tab_mojom->training_request_id =
            url_visit_aggregate.request_id.GetUnsafeValue();
        if (IsNewURL(history_tab_mojom)) {
          tabs_mojom.push_back(std::move(history_tab_mojom));
          base::UmaHistogramEnumeration(
              "NewTabPage.TabResumption.URLVisitAggregateDataTypeDisplayed",
              URLVisitAggregateDataType::kHistory);
        }
      }
    }
  }

  std::move(callback).Run(std::move(tabs_mojom));
}

// static
void MostRelevantTabResumptionPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedTabsPrefName, base::Value::List());
}

bool MostRelevantTabResumptionPageHandler::IsNewURL(
    history::mojom::TabPtr& tab) {
  const base::Value::List& cached_urls =
      profile_->GetPrefs()->GetList(kDismissedTabsPrefName);
  auto it =
      std::find_if(cached_urls.begin(), cached_urls.end(),
                   [&tab](const base::Value& cached_url) {
                     return cached_url.GetString() ==
                            tab->url_key + ' ' +
                                base::NumberToString(
                                    tab->timestamp->ToDeltaSinceWindowsEpoch()
                                        .InMicroseconds());
                   });
  return it == cached_urls.end();
}

void MostRelevantTabResumptionPageHandler::RemoveOldDismissedTabs() {
  ScopedListPrefUpdate tab_list(profile_->GetPrefs(), kDismissedTabsPrefName);
  for (const auto& entry : tab_list.Get().Clone()) {
    const std::string dismissed_tab_string = entry.GetString();
    size_t delimiter_pos = dismissed_tab_string.find(' ');
    if (delimiter_pos != std::string::npos) {
      int64_t timestamp_microseconds;
      base::StringToInt64(dismissed_tab_string.substr(delimiter_pos),
                          &timestamp_microseconds);
      base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(timestamp_microseconds));
      if (base::Time::Now() - timestamp >
          base::Days(dismissal_duration_days_)) {
        tab_list->EraseValue(entry);
      }
    }
  }
}

void MostRelevantTabResumptionPageHandler::RecordAction(
    ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction action,
    const std::string& url_key,
    int64_t visit_request_id) {
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking::ScoredURLUserAction user_action;
  switch (action) {
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kUnknown:
      user_action = visited_url_ranking::ScoredURLUserAction::kUnknown;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::kSeen:
      user_action = visited_url_ranking::ScoredURLUserAction::kSeen;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kActivated:
      user_action = visited_url_ranking::ScoredURLUserAction::kActivated;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kDismissed:
      user_action = visited_url_ranking::ScoredURLUserAction::kDismissed;
      break;
  }
  visited_url_ranking_service->RecordAction(
      user_action, url_key,
      segmentation_platform::TrainingRequestId(visit_request_id));
}
