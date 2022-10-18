// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/log_test_helpers.h"

#include <string>
#include <vector>

#include "base/strings/string_split.h"

namespace ash {
namespace diagnostics {

const char kSeparator[] = "-";
const char kNewline[] = "\n";

std::vector<std::string> GetLogLines(const std::string& log) {
  return base::SplitString(log, kNewline,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> GetLogLineContents(const std::string& log_line,
                                            const std::string& separators) {
  const std::vector<std::string> result = base::SplitString(
      log_line, separators, base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return result;
}

}  // namespace diagnostics
}  // namespace ash
