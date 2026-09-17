// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_util.h"

#include "base/time/time.h"

namespace ash {
namespace {

// The prefix of a drive file suggestion id.
constexpr char kDriveFileSuggestionPrefix[] = "zero_state_drive://";

// The prefix of a local file suggestion id.
constexpr char kLocalFileSuggestionPrefix[] = "zero_state_file://";

// The number of days within which a file must be modified, or viewed to be
// considered as a file suggestion.
constexpr int kDefaultMaxRecencyInDays = 30;

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
  return base::Days(kDefaultMaxRecencyInDays);
}

// FileSuggestData -------------------------------------------------------------

FileSuggestData::FileSuggestData(
    FileSuggestionType new_type,
    const base::FilePath& new_file_path,
    const std::optional<std::string>& title,
    const std::optional<std::u16string>& new_prediction_reason,
    const std::optional<base::Time>& modified_time,
    const std::optional<base::Time>& viewed_time,
    const std::optional<base::Time>& shared_time,
    std::optional<float> new_score,
    const std::optional<std::string>& drive_file_id,
    const std::optional<std::string>& icon_url)
    : type(new_type),
      file_path(new_file_path),
      title(title),
      id(CalculateSuggestionId(type, file_path)),
      prediction_reason(new_prediction_reason),
      modified_time(modified_time),
      viewed_time(viewed_time),
      shared_time(shared_time),
      score(new_score),
      drive_file_id(drive_file_id),
      icon_url(icon_url) {}

FileSuggestData::FileSuggestData(FileSuggestData&&) = default;

FileSuggestData::FileSuggestData(const FileSuggestData&) = default;

FileSuggestData& FileSuggestData::operator=(const FileSuggestData&) = default;

FileSuggestData::~FileSuggestData() = default;

// Helper functions ------------------------------------------------------------

// Calculates the ID used to remove suggestions the user doesn't want to see.
std::string CalculateSuggestionId(FileSuggestionType type,
                                  const base::FilePath& file_path) {
  return GetPrefixFromSuggestionType(type) + file_path.value();
}

}  // namespace ash
