// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_activity_types_service.h"

#include <unordered_set>

#include "base/containers/fixed_flat_map.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {

PrivacySandboxActivityTypesService::PrivacySandboxActivityTypesService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

PrivacySandboxActivityTypesService::~PrivacySandboxActivityTypesService() =
    default;

void PrivacySandboxActivityTypesService::Shutdown() {
  pref_service_ = nullptr;
}

void RecordPercentageMetrics(const base::Value::List& activity_type_record) {
  using ActivityType =
      PrivacySandboxActivityTypesService::PrivacySandboxStorageActivityType;
  std::unordered_map<ActivityType, int> activity_type_counts{
      {ActivityType::kOther, 0},
      {ActivityType::kTabbed, 0},
      {ActivityType::kAGSACustomTab, 0},
      {ActivityType::kNonAGSACustomTab, 0},
      {ActivityType::kTrustedWebActivity, 0},
      {ActivityType::kWebapp, 0},
      {ActivityType::kWebApk, 0},
      {ActivityType::kPreFirstTab, 0}};

  for (const base::Value& record : activity_type_record) {
    std::optional<int> activity_type_int =
        record.GetDict().FindInt("activity_type");
    CHECK(activity_type_int.has_value());
    ActivityType activity_type =
        static_cast<ActivityType>(activity_type_int.value());
    activity_type_counts[activity_type]++;
  }

  std::unordered_map<ActivityType, int> activity_type_percentages;
  // Set each activity type percentage based on the count / total_records.
  for (const auto& [key, value] : activity_type_counts) {
    double raw_percentage = (value * 100.0) / activity_type_record.size();
    activity_type_percentages[key] = std::round(raw_percentage);
  }

  constexpr auto kTypesToHistogramSuffix =
      base::MakeFixedFlatMap<ActivityType, std::string_view>(
          {{ActivityType::kOther, "Other"},
           {ActivityType::kTabbed, "BrApp"},
           {ActivityType::kAGSACustomTab, "AGSACCT"},
           {ActivityType::kNonAGSACustomTab, "NonAGSACCT"},
           {ActivityType::kTrustedWebActivity, "TWA"},
           {ActivityType::kWebapp, "WebApp"},
           {ActivityType::kWebApk, "WebApk"},
           {ActivityType::kPreFirstTab, "PreFirstTab"}});

  // Emit all the histograms with each percentage value.
  for (const auto& [type, suffix] : kTypesToHistogramSuffix) {
    if (!activity_type_percentages.contains(type)) {
      return;
    }
    base::UmaHistogramPercentage(
        base::StrCat(
            {"PrivacySandbox.ActivityTypeStorage.Percentage.", suffix, "2"}),
        activity_type_percentages[type]);
  }
}

void RecordUserSegmentMetrics(const base::Value::List& activity_type_record,
                              int records_in_a_row) {
  // If a different value for records_in_a_row is needed for these metrics,
  // tools/metrics/histograms/metadata/privacy/histograms.xml needs to be
  // updated with new histograms. Currently, only
  // 10MostRecentRecordsUserSegment2 and 20MostRecentRecordsUserSegment2
  // histograms are necessary.
  DCHECK(records_in_a_row == 10 || records_in_a_row == 20);
  // Can't emit user segment metrics when the size of the list is less than
  // records_in_a_row
  if (activity_type_record.size() < static_cast<size_t>(records_in_a_row)) {
    return;
  }
  using ActivityType =
      PrivacySandboxActivityTypesService::PrivacySandboxStorageActivityType;
  using SegmentType = PrivacySandboxActivityTypesService::
      PrivacySandboxStorageUserSegmentByRecentActivity;

  // Helper function to get the activity type from a base::Value
  auto GetActivityType = [](const base::Value& record) -> ActivityType {
    std::optional<int> activity_type_int =
        record.GetDict().FindInt("activity_type");
    CHECK(activity_type_int.has_value());
    return static_cast<ActivityType>(activity_type_int.value());
  };

  std::unordered_set<ActivityType> encountered_activities;
  for (int i = 0; i < records_in_a_row; ++i) {
    encountered_activities.insert(GetActivityType(activity_type_record[i]));
  }

  SegmentType segment_type = SegmentType::kHasOther;
  if (encountered_activities.contains(ActivityType::kTabbed)) {
    segment_type = SegmentType::kHasBrowserApp;
  } else if (encountered_activities.contains(ActivityType::kAGSACustomTab)) {
    segment_type = SegmentType::kHasAGSACCT;
  } else if (encountered_activities.contains(ActivityType::kNonAGSACustomTab)) {
    segment_type = SegmentType::kHasNonAGSACCT;
  } else if (encountered_activities.contains(ActivityType::kWebApk)) {
    segment_type = SegmentType::kHasPWA;
  } else if (encountered_activities.contains(
                 ActivityType::kTrustedWebActivity)) {
    segment_type = SegmentType::kHasTWA;
  } else if (encountered_activities.contains(ActivityType::kWebapp)) {
    segment_type = SegmentType::kHasWebapp;
  } else if (encountered_activities.contains(ActivityType::kPreFirstTab)) {
    segment_type = SegmentType::kHasPreFirstTab;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.ActivityTypeStorage.",
                    base::NumberToString(records_in_a_row),
                    "MostRecentRecordsUserSegment2"}),
      segment_type);
}

void RecordDaysSinceMetrics(const base::Value::List& activity_type_record) {
  auto* timestamp =
      activity_type_record[activity_type_record.size() - 1].GetDict().Find(
          "timestamp");
  CHECK(timestamp);
  std::optional<base::Time> oldest_record_timestamp =
      base::ValueToTime(*timestamp);
  CHECK(oldest_record_timestamp.has_value());
  int days_since_oldest_record =
      (base::Time::Now() - oldest_record_timestamp.value()).InDays();
  base::UmaHistogramCustomCounts(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord",
      days_since_oldest_record, 1, 61, 60);
}

void RecordActivityTypeMetrics(const base::Value::List& activity_type_record,
                               base::Time current_time) {
  int total_records = static_cast<int>(activity_type_record.size());
  auto* oldest_record_timestamp_ptr =
      activity_type_record[total_records - 1].GetDict().Find("timestamp");
  CHECK(oldest_record_timestamp_ptr);
  std::optional<base::Time> oldest_record_timestamp =
      base::ValueToTime(*oldest_record_timestamp_ptr);
  CHECK(oldest_record_timestamp.has_value());
  base::Time uma_enabled_timestamp =
      base::Time::FromTimeT(g_browser_process->local_state()->GetInt64(
          metrics::prefs::kMetricsReportingEnabledTimestamp));
  // If a user has opted in, but the opt-in date is after the oldest record
  // timestamp in the activity type list, then no metrics should be emitted.
  if (oldest_record_timestamp.value() < uma_enabled_timestamp) {
    return;
  }
  // Min: 1, Max: 201 (exclusive), Buckets: 200 (in case the max total records
  // changes from 100).
  base::UmaHistogramCustomCounts(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength",
      static_cast<int>(activity_type_record.size()), 1, 201, 200);
  RecordPercentageMetrics(activity_type_record);
  RecordUserSegmentMetrics(activity_type_record, 10);
  RecordUserSegmentMetrics(activity_type_record, 20);
  RecordDaysSinceMetrics(activity_type_record);
}

void PrivacySandboxActivityTypesService::RecordActivityType(
    PrivacySandboxStorageActivityType type) const {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived", type);

  // If skip-pre-first-tab is turned on, the list is not updated when the type
  // passed in is kPreFirstTab.
  if (type == PrivacySandboxActivityTypesService::
                  PrivacySandboxStorageActivityType::kPreFirstTab &&
      privacy_sandbox::kPrivacySandboxActivityTypeStorageSkipPreFirstTab
          .Get()) {
    return;
  }

  // Activity type launches can only be recorded if they fall within a specific
  // timeframe. This timeframe is determined by the within-x-days parameter,
  // where oldest_timestamp_allowed marks the end of the timeframe and
  // current_time marks the beginning.
  base::Time current_time = base::Time::Now();
  base::Time oldest_timestamp_allowed =
      current_time -
      base::Days(
          privacy_sandbox::kPrivacySandboxActivityTypeStorageWithinXDays.Get());

  base::Value::Dict new_dict;
  new_dict.Set("timestamp", base::TimeToValue(current_time));
  new_dict.Set("activity_type", static_cast<int>(type));

  const base::Value::List& old_activity_type_record =
      pref_service_->GetList(prefs::kPrivacySandboxActivityTypeRecord2);

  base::Value::List new_activity_type_record;
  new_activity_type_record.Append(std::move(new_dict));

  int last_n_launches =
      privacy_sandbox::kPrivacySandboxActivityTypeStorageLastNLaunches.Get();
  // The list is ordered from most recent records in the beginning of the list
  // and old records at the end of the list.
  for (const base::Value& child : old_activity_type_record) {
    const base::Value* child_timestamp_ptr = child.GetDict().Find("timestamp");
    if (!child_timestamp_ptr) {
      continue;
    }
    std::optional<base::Time> child_timestamp =
        base::ValueToTime(*child_timestamp_ptr);
    if (!child_timestamp.has_value()) {
      continue;
    }
    if (current_time >= child_timestamp.value() &&
        child_timestamp.value() >= oldest_timestamp_allowed &&
        new_activity_type_record.size() <
            static_cast<size_t>(last_n_launches)) {
      new_activity_type_record.Append(child.Clone());
    }
  }
  RecordActivityTypeMetrics(new_activity_type_record, current_time);
  pref_service_->SetList(prefs::kPrivacySandboxActivityTypeRecord2,
                         std::move(new_activity_type_record));
}
}  // namespace privacy_sandbox
