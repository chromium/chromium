// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_

#include <optional>
#include <string>

#include "base/time/time.h"

namespace ash {
enum class FileSuggestionJustificationType;
}  // namespace ash

namespace app_list {

// Returns a justification string for file suggestions. The justification string
// describes the action that prompted the file to be suggested to the user.
// `type` is the type of action that prompted the suggestion.
// `timestamp` is the time the action occurred.
// `user_name` is the name of the user that performed the action. The user name
// is only relevant for `kModified` and `kShared` actions (otherwise the action
// is presumed to be performed by the current user). It can be an empty string
// if the user name is not relevant for the action, or not known (in which case
// the justification will show a fallback string without user name).
std::optional<std::u16string> GetJustificationString(
    ash::FileSuggestionJustificationType type,
    const base::Time& timestamp,
    const std::string& user_name);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
