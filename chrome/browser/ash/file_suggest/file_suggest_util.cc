// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_util.h"

#include "ash/constants/ash_features.h"
#include "base/time/time.h"

namespace ash {
namespace {

// The prefix of a drive file suggestion id.
constexpr char kDriveFileSuggestionPrefix[] = "zero_state_drive://";

// The prefix of a local file suggestion id.
constexpr char kLocalFileSuggestionPrefix[] = "zero_state_file://";

// The number of days within which a file must be modified, or viewed to be
// considered as a file suggestion.
constexpr int kDefaultMaxRecencyInDays = 8;

// Returns the prefix that matches `type`.
std::string GetPrefixFromSuggestionType(FileSuggestionType type) {
  switch (type) {
    case FileSuggestionType::kDriveFile:
      return kDriveFileSuggestionPrefix;
    case FileSuggestionType::kLocalFile:
      return kLocalFileSuggestionPrefix;
  }
}

}  // namespace

base::TimeDelta GetMaxFileSuggestionRecency() {
  if (base::FeatureList::IsEnabled(
          features::kLauncherContinueSectionWithRecents)) {
    return base::Days(base::GetFieldTrialParamByFeatureAsInt(
        features::kLauncherContinueSectionWithRecents, "max_recency_in_days",
        kDefaultMaxRecencyInDays));
  }

  return base::Days(base::GetFieldTrialParamByFeatureAsInt(
      features::kLauncherContinueSectionWithRecentsRollout,
      "max_recency_in_days", kDefaultMaxRecencyInDays));
}

double ToTimestampBasedScore(const FileSuggestData& data,
                             base::TimeDelta max_recency) {
  auto score_timestamp = [&](const base::Time& timestamp, double interval_max,
                             double interval_size) {
    return interval_max -
           interval_size *
               std::min(
                   1.0,
                   (base::Time::Now() - timestamp).magnitude().InSeconds() /
                       static_cast<double>(max_recency.InSeconds()));
  };

  if (data.timestamp) {
    return score_timestamp(*data.timestamp,
                           /*interval_max=*/1.0, /*interval_size=*/0.5);
  }

  if (data.secondary_timestamp) {
    return score_timestamp(*data.secondary_timestamp,
                           /*interval_max=*/0.5, /*interval_size=*/0.5);
  }
  return 0.0;
}

// FileSuggestData -------------------------------------------------------------

FileSuggestData::FileSuggestData(
    FileSuggestionType new_type,
    const base::FilePath& new_file_path,
    FileSuggestionJustificationType justification_type,
    const std::optional<std::u16string>& new_prediction_reason,
    const std::optional<base::Time>& timestamp,
    const std::optional<base::Time>& secondary_timestamp,
    std::optional<float> new_score)
    : type(new_type),
      file_path(new_file_path),
      id(CalculateSuggestionId(type, file_path)),
      justification_type(justification_type),
      prediction_reason(new_prediction_reason),
      timestamp(timestamp),
      secondary_timestamp(secondary_timestamp),
      score(new_score) {}

FileSuggestData::FileSuggestData(FileSuggestData&&) = default;

FileSuggestData::FileSuggestData(const FileSuggestData&) = default;

FileSuggestData& FileSuggestData::operator=(const FileSuggestData&) = default;

FileSuggestData::~FileSuggestData() = default;

// Helper functions ------------------------------------------------------------

std::string CalculateSuggestionId(FileSuggestionType type,
                                  const base::FilePath& file_path) {
  return GetPrefixFromSuggestionType(type) + file_path.value();
}

}  // namespace ash
