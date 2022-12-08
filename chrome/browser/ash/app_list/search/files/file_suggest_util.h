// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

struct FileSuggestData;
using GetSuggestFileDataCallback = base::OnceCallback<void(
    const absl::optional<std::vector<FileSuggestData>>&)>;

// Outcome of a call to `FileSuggestKeyedService::GetSuggestFileData()`. These
// values persist to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class DriveSuggestValidationStatus {
  kOk = 0,
  kDriveFSNotMounted = 1,
  kNoResults = 2,
  kPathLocationFailed = 3,
  kAllFilesErrored = 4,
  kDriveDisabled = 5,
  kMaxValue = kDriveDisabled,
};

// The types of the file suggestion data, ordered by decreasing precedence.
enum class FileSuggestionType {
  // The drive file suggestion.
  kDriveFile,

  // The local file suggestion.
  kLocalFile,
};

// The data of an individual file suggested by `FileSuggestKeyedService`.
struct FileSuggestData {
  FileSuggestData(FileSuggestionType new_type,
                  const base::FilePath& new_file_path,
                  const absl::optional<std::u16string>& new_prediction_reason,
                  absl::optional<float> new_score);
  FileSuggestData(FileSuggestData&&);
  FileSuggestData(const FileSuggestData&);
  FileSuggestData& operator=(const FileSuggestData&);
  ~FileSuggestData();

  // The type of the suggested file (e.g. a drive file).
  FileSuggestionType type;

  // The path to the suggested file.
  base::FilePath file_path;

  // The suggestion id. Calculated from `type` and `file_path`.
  std::string id;

  // The reason why the file is suggested.
  absl::optional<std::u16string> prediction_reason;

  // Only has a value when `type` == `FileSuggestionType::kLocalFile`.
  absl::optional<float> score;
};

// Calculates the id of a file suggestion specified by `type` and `file_path`.
std::string CalculateSuggestionId(FileSuggestionType type,
                                  const base::FilePath& file_path);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_UTIL_H_
