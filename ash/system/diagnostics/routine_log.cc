// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/routine_log.h"

#include <sstream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
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

std::string GetRoutineLogCategoryString(RoutineLog::RoutineCategory category) {
  switch (category) {
    case RoutineLog::RoutineCategory::kNetwork:
      return "network";
    case RoutineLog::RoutineCategory::kSystem:
      return "system";
  }
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
RoutineLog::RoutineCategory GetRoutineCategory(mojom::RoutineType type) {
  switch (type) {
    case mojom::RoutineType::kBatteryCharge:
    case mojom::RoutineType::kBatteryDischarge:
    case mojom::RoutineType::kCpuCache:
    case mojom::RoutineType::kCpuStress:
    case mojom::RoutineType::kCpuFloatingPoint:
    case mojom::RoutineType::kCpuPrime:
    case mojom::RoutineType::kMemory:
      return RoutineLog::RoutineCategory::kSystem;
    case mojom::RoutineType::kLanConnectivity:
    case mojom::RoutineType::kSignalStrength:
    case mojom::RoutineType::kGatewayCanBePinged:
    case mojom::RoutineType::kHasSecureWiFiConnection:
    case mojom::RoutineType::kDnsResolverPresent:
    case mojom::RoutineType::kDnsLatency:
    case mojom::RoutineType::kDnsResolution:
    case mojom::RoutineType::kCaptivePortal:
    case mojom::RoutineType::kHttpFirewall:
    case mojom::RoutineType::kHttpsFirewall:
    case mojom::RoutineType::kHttpsLatency:
    case mojom::RoutineType::kArcHttp:
    case mojom::RoutineType::kArcPing:
    case mojom::RoutineType::kArcDnsResolution:
      return RoutineLog::RoutineCategory::kNetwork;
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
    const RoutineCategory category) const {
  const auto iter = logs_.find(category);
  if (iter == logs_.end()) {
    return "";
  }

  return iter->second->GetContents();
}

void RoutineLog::Append(mojom::RoutineType type, const std::string& text) {
  RoutineCategory category = GetRoutineCategory(type);

  // Insert a new log if it doesn't exist then append to it.
  base::FilePath log_path = GetCategoryLogFilePath(category);
  auto iter = logs_.find(category);
  if (iter == logs_.end()) {
    iter = logs_.emplace(category, std::make_unique<AsyncLog>(log_path)).first;
  }

  iter->second->Append(text);
}

base::FilePath RoutineLog::GetCategoryLogFilePath(
    const RoutineCategory category) {
  std::string name =
      "diagnostics_routines_" + GetRoutineLogCategoryString(category) + ".log";
  return log_base_path_.Append(name);
}

}  // namespace diagnostics
}  // namespace ash
