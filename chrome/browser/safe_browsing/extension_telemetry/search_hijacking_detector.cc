// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/search_hijacking_detector.h"

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_service.h"

namespace safe_browsing {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SearchVsSerpCount)
enum class SearchVsSerpCount {
  kSearchCountIsLessThanSerpCount = 0,
  kSearchCountEqualsSerpCount = 1,
  kSearchCountIsGreaterThanSerpCount = 2,
  kMaxValue = kSearchCountIsGreaterThanSerpCount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SBExtensionTelemetrySearchVsSerpCount)

void RecordHistograms(int search_count, int serp_count, bool heuristic_match) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "HeuristicMatch",
      heuristic_match);

  if (search_count == 0 && serp_count == 0) {
    return;
  }

  SearchVsSerpCount sample;
  if (search_count < serp_count) {
    sample = SearchVsSerpCount::kSearchCountIsLessThanSerpCount;
  } else if (search_count == serp_count) {
    sample = SearchVsSerpCount::kSearchCountEqualsSerpCount;
  } else {  // search_count > serp_count
    sample = SearchVsSerpCount::kSearchCountIsGreaterThanSerpCount;
  }

  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.SearchHijackingDetector."
      "SearchVsSerpCount",
      sample);
}

}  // namespace

SearchHijackingDetector::SearchHijackingDetector(
    PrefService* pref_service,
    TemplateURLService* template_url_service)
    : pref_service_(pref_service),
      template_url_service_(template_url_service) {}

SearchHijackingDetector::~SearchHijackingDetector() = default;

void SearchHijackingDetector::OnOmniboxSearch(const AutocompleteMatch& match) {
  if (template_url_service_ &&
      template_url_service_->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    int count = pref_service_->GetInteger(
        prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount);
    pref_service_->SetInteger(
        prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount, count + 1);
  }
}

void SearchHijackingDetector::OnDseSerpLoaded() {
  int count = pref_service_->GetInteger(
      prefs::kExtensionTelemetrySearchHijackingSerpLandingCount);
  pref_service_->SetInteger(
      prefs::kExtensionTelemetrySearchHijackingSerpLandingCount, count + 1);
}

void SearchHijackingDetector::MaybeCheckForHeuristicMatch() {
  base::Time last_check_time = pref_service_->GetTime(
      prefs::kExtensionTelemetrySearchHijackingLastCheckTime);
  if (base::Time::Now() - last_check_time < heuristic_check_interval_) {
    return;
  }

  int search_count = pref_service_->GetInteger(
      prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount);
  int serp_count = pref_service_->GetInteger(
      prefs::kExtensionTelemetrySearchHijackingSerpLandingCount);

  bool heuristic_match = false;
  if (search_count - serp_count >= heuristic_threshold_) {
    heuristic_match = true;
    base::Value::Dict signal_data;
    signal_data.Set(
        "detection_timestamp",
        base::NumberToString(base::Time::Now().InMillisecondsSinceUnixEpoch()));
    signal_data.Set("omnibox_search_count", search_count);
    signal_data.Set("serp_landing_count", serp_count);
    pref_service_->SetDict(prefs::kExtensionTelemetrySearchHijackingSignalData,
                           std::move(signal_data));
  }

  RecordHistograms(search_count, serp_count, heuristic_match);

  pref_service_->SetTime(prefs::kExtensionTelemetrySearchHijackingLastCheckTime,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount, 0);
  pref_service_->SetInteger(
      prefs::kExtensionTelemetrySearchHijackingSerpLandingCount, 0);
}

std::unique_ptr<ExtensionTelemetryReportRequest_SearchHijackingSignal>
SearchHijackingDetector::GetSignalForReport() {
  if (!pref_service_->HasPrefPath(
          prefs::kExtensionTelemetrySearchHijackingSignalData)) {
    return nullptr;
  }

  const base::Value::Dict& signal_data = pref_service_->GetDict(
      prefs::kExtensionTelemetrySearchHijackingSignalData);
  auto signal = std::make_unique<
      ExtensionTelemetryReportRequest::SearchHijackingSignal>();
  const std::string* timestamp_str =
      signal_data.FindString("detection_timestamp");
  int64_t timestamp = 0;
  if (!base::StringToInt64(timestamp_str ? *timestamp_str : "", &timestamp)) {
    // `timestamp` is written even if string conversion fn. returns false, so 0
    // it out.
    timestamp = 0;
  }
  signal->set_detection_timestamp(timestamp);
  signal->set_omnibox_search_count(
      signal_data.FindInt("omnibox_search_count").value_or(0));
  signal->set_serp_landing_count(
      signal_data.FindInt("serp_landing_count").value_or(0));
  return signal;
}

void SearchHijackingDetector::ClearAllDataFromPrefs() {
  pref_service_->ClearPref(
      prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount);
  pref_service_->ClearPref(
      prefs::kExtensionTelemetrySearchHijackingSerpLandingCount);
  pref_service_->ClearPref(prefs::kExtensionTelemetrySearchHijackingSignalData);
  pref_service_->ClearPref(
      prefs::kExtensionTelemetrySearchHijackingLastCheckTime);
}

SearchHijackingDetector::EventCounts
SearchHijackingDetector::GetCurrentEventCountsForTesting() {
  return {pref_service_->GetInteger(
              prefs::kExtensionTelemetrySearchHijackingOmniboxSearchCount),
          pref_service_->GetInteger(
              prefs::kExtensionTelemetrySearchHijackingSerpLandingCount)};
}

base::Time SearchHijackingDetector::GetLastHeuristicCheckTimeForTesting() {
  return pref_service_->GetTime(
      prefs::kExtensionTelemetrySearchHijackingLastCheckTime);
}

void SearchHijackingDetector::SetHeuristicCheckInterval(
    base::TimeDelta interval) {
  heuristic_check_interval_ = interval;
}

base::TimeDelta SearchHijackingDetector::GetHeuristicCheckInterval() const {
  return heuristic_check_interval_;
}

void SearchHijackingDetector::SetHeuristicThreshold(int threshold) {
  heuristic_threshold_ = threshold;
}

int SearchHijackingDetector::GetHeuristicThreshold() const {
  return heuristic_threshold_;
}

}  // namespace safe_browsing
