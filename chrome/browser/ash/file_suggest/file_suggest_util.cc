// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_util.h"

namespace ash {
namespace {

// The prefix of a drive file suggestion id.
constexpr char kDriveFileSuggestionPrefix[] = "zero_state_drive://";

// The prefix of a local file suggestion id.
constexpr char kLocalFileSuggestionPrefix[] = "zero_state_file://";

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

// FileSuggestData -------------------------------------------------------------

FileSuggestData::FileSuggestData(
    FileSuggestionType new_type,
    const base::FilePath& new_file_path,
    const absl::optional<std::u16string>& new_prediction_reason,
    absl::optional<float> new_score)
    : type(new_type),
      file_path(new_file_path),
      id(CalculateSuggestionId(type, file_path)),
      prediction_reason(new_prediction_reason),
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
