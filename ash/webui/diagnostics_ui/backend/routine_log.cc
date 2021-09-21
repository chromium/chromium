// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/routine_log.h"

#include <sstream>
#include <string>

#include "base/files/file_path.h"
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

}  // namespace

RoutineLog::RoutineLog(const base::FilePath& routine_log_file_path)
    : log_(routine_log_file_path) {}

RoutineLog::~RoutineLog() = default;

void RoutineLog::LogRoutineStarted(mojom::RoutineType type) {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator << kStartedDescription
           << kNewline;
  log_.Append(log_line.str());
}

void RoutineLog::LogRoutineCompleted(mojom::RoutineType type,
                                     mojom::StandardRoutineResult result) {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator
           << getRoutineResultString(result) << kNewline;
  log_.Append(log_line.str());
}

void RoutineLog::LogRoutineCancelled() {
  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << kCancelledDescription << kNewline;
  log_.Append(log_line.str());
}

std::string RoutineLog::GetContents() const {
  return log_.GetContents();
}

}  // namespace diagnostics
}  // namespace ash
