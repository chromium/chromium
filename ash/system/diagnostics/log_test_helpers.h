// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_LOG_TEST_HELPERS_H_
#define ASH_SYSTEM_DIAGNOSTICS_LOG_TEST_HELPERS_H_

#include <string>
#include <vector>

namespace ash {
namespace diagnostics {

extern const char kSeparator[];
extern const char kNewline[];

// Returns the lines of the log as a vector of strings.
std::vector<std::string> GetLogLines(const std::string& log);

// Splits a single line of the log at `kSeparator`.
std::vector<std::string> GetLogLineContents(
    const std::string& log_line,
    const std::string& separators = kSeparator);

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_LOG_TEST_HELPERS_H_
