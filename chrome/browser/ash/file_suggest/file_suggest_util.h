// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_UTIL_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ash {

struct FileSuggestData;
using GetSuggestFileDataCallback = base::OnceCallback<void(
    const std::optional<std::vector<FileSuggestData>>&)>;

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

// The reason the file is suggested.
enum class FileSuggestionJustificationType {
  // Used only for deprecated suggestions from drive's ItemSuggest API, for
  // which the exact justification type is obscured.
  kUnknown,

  // The user recently viewed the file.
  kViewed,

  // The file was recently modified, usually by another user.
  kModified,

  // The file was recently modified by the current user.
  kModifiedByCurrentUser,

  // The file was shared with the current user.
  kShared,
};

// Returns the max amount of time from now that a file was modified or viewed to
// be available as a file suggestion.
base::TimeDelta GetMaxFileSuggestionRecency();

// Creates a suggestion score in interval [0, 1] based on the suggestion
// timestamps. Assumes that the timestamps are at most `max_recency` old.
double ToTimestampBasedScore(const FileSuggestData& suggestion_data,
                             base::TimeDelta max_recency);

// The data of an individual file suggested by `FileSuggestKeyedService`.
struct FileSuggestData {
  FileSuggestData(FileSuggestionType new_type,
                  const base::FilePath& new_file_path,
                  const std::optional<std::string>& title,
                  const std::optional<std::u16string>& new_prediction_reason,
                  const std::optional<base::Time>& modified_time,
                  const std::optional<base::Time>& viewed_time,
                  const std::optional<base::Time>& shared_time,
                  std::optional<float> new_score,
                  const std::optional<std::string>& drive_file_id,
                  const std::optional<std::string>& icon_url);
  FileSuggestData(FileSuggestData&&);
  FileSuggestData(const FileSuggestData&);
  FileSuggestData& operator=(const FileSuggestData&);
  ~FileSuggestData();

  // The type of the suggested file (e.g. a drive file).
  FileSuggestionType type;

  // The path to the suggested file.
  base::FilePath file_path;

  // The title of the file. The title can contain path separators like "/",
  // which is the reason that this title member is used instead of the file name
  // in `file_path`.
  std::optional<std::string> title;

  // The suggestion id. Calculated from `type` and `file_path`.
  std::string id;

  // The reason why the file is suggested.
  std::optional<std::u16string> prediction_reason;

  // Timestamp of when the file was last modified by the user.
  std::optional<base::Time> modified_time;

  // Timestamp of when the file was last viewed by the user.
  std::optional<base::Time> viewed_time;

  // Timestamp of when the file was shared with the user.
  std::optional<base::Time> shared_time;

  // Only has a value when `type` == `FileSuggestionType::kLocalFile`.
  std::optional<float> score;

  // The drive file id for the file.
  std::optional<std::string> drive_file_id;

  // The custom icon url for the file.
  std::optional<std::string> icon_url;
};

// Calculates the id of a file suggestion specified by `type` and `file_path`.
std::string CalculateSuggestionId(FileSuggestionType type,
                                  const base::FilePath& file_path);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_UTIL_H_
