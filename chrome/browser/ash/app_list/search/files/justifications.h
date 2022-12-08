// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_

#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

// Returns an appropriate justification string for the given times, for example
// "Opened yesterday". There are different messages for times within the last
// few minutes, day, two days, week, and month. If the time is longer than a
// month ago, nullopt is returned.
absl::optional<std::u16string> GetJustificationString(
    const base::Time& last_accessed,
    const base::Time& last_modified);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
