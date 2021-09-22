// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/routine_log.h"

#include <sstream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace {

const char kCancelledDescription[] = "Inflight Routine Cancelled";
const char kNewline[] = "\n";
const char kSeparator[] = " - ";
const char kStartedDescription[] = "Started";

std::string GetCurrentDateTimeAsString() {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(base::Time::Now()));
}

std::string getRoutineResultString(mojom::StandardRoutineResult result) {
  switch (result) {
    case mojom::StandardRoutineResult::kTestPassed:
      return "Passed";
    case mojom::StandardRoutineResult::kTestFailed:
      return "Failed";
    case mojom::StandardRoutineResult::kExecutionError:
      return "Execution error";
    case mojom::StandardRoutineResult::kUnableToRun:
      return "Unable to run";
  }
}

std::string getRoutineTypeString(mojom::RoutineType type) {
  std::stringstream s;
  s << type;
  const std::string routineName = s.str();

  // Remove leading "k" ex: "kCpuStress" -> "CpuStress".
  DCHECK_GE(routineName.size(), 1U);
  DCHECK_EQ(routineName[0], 'k');
  return routineName.substr(1, routineName.size() - 1);
}

// Get the category for the routine `type`.
std::string GetRoutineCategory(mojom::RoutineType type) {
  switch (type) {
    // TODO(zentaro): Implement categorization.
    default:
      return "all";
  };
}

}  // namespace

RoutineLog::RoutineLog(const base::FilePath& log_base_path)
    : log_base_path_(log_base_path) {}

RoutineLog::~RoutineLog() = default;

void RoutineLog::LogRoutineStarted(mojom::RoutineType type) {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator << kStartedDescription
           << kNewline;
  Append(type, log_line.str());
}

void RoutineLog::LogRoutineCompleted(mojom::RoutineType type,
                                     mojom::StandardRoutineResult result) {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator
           << getRoutineResultString(result) << kNewline;
  Append(type, log_line.str());
}

void RoutineLog::LogRoutineCancelled(mojom::RoutineType type) {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << kCancelledDescription << kNewline;
  Append(type, log_line.str());
}

std::string RoutineLog::GetContentsForCategory(
    const std::string& category) const {
  // TODO(zentaro): Remove DCHECK once categories are implemented.
  DCHECK_EQ("all", category);

  const auto iter = logs_.find(category);
  if (iter == logs_.end()) {
    return "";
  }

  return iter->second->GetContents();
}

void RoutineLog::Append(mojom::RoutineType type, const std::string& text) {
  std::string category = GetRoutineCategory(type);

  // TODO(zentaro): Remove DCHECK once categories are implemented.
  DCHECK_EQ("all", category);

  // Insert a new log if it doesn't exist then append to it.
  base::FilePath log_path = GetCategoryLogFilePath(category);
  auto iter = logs_.find(category);
  if (iter == logs_.end()) {
    iter = logs_.emplace(category, std::make_unique<AsyncLog>(log_path)).first;
  }

  iter->second->Append(text);
}

base::FilePath RoutineLog::GetCategoryLogFilePath(const std::string& category) {
  std::string name = "diagnostics_routines_" + category + ".log";
  return log_base_path_.Append(name);
}

}  // namespace diagnostics
}  // namespace ash
