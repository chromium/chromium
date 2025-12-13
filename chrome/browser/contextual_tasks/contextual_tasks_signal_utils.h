// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SIGNAL_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SIGNAL_UTILS_H_

#include <string>

namespace contextual_tasks {

// Gets number of matching words between `query` text and `candidate` text,
// ignoring the matches on stop words.
int GetMatchingWordsCount(const std::string& query,
                          const std::string& candidate);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SIGNAL_UTILS_H_
