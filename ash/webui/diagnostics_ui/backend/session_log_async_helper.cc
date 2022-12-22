// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/session_log_async_helper.h"

#include <memory>
#include <string>

#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"

namespace ash::diagnostics {

namespace {

const char kRoutineLogSubsectionHeader[] = "--- Test Routines --- \n";
const char kSystemLogSectionHeader[] = "=== System === \n";
const char kNetworkingLogSectionHeader[] = "=== Networking === \n";
const char kNoRoutinesRun[] =
    "No routines of this type were run in the session.\n";

std::string GetRoutineResultsString(const std::string& results) {
  const std::string section_header =
      std::string(kRoutineLogSubsectionHeader) + "\n";
  if (results.empty()) {
    return section_header + kNoRoutinesRun;
  }

  return section_header + results;
}

}  // namespace

SessionLogAsyncHelper::SessionLogAsyncHelper() = default;
SessionLogAsyncHelper::~SessionLogAsyncHelper() = default;

bool SessionLogAsyncHelper::CreateSessionLogOnBlockingPool(
    const base::FilePath file_path,
    TelemetryLog* telemetry_log,
    RoutineLog* routine_log,
    NetworkingLog* networking_log) {
  // Fetch Routine logs
  const std::string system_routines =
      routine_log ? routine_log->GetContentsForCategory(
                        RoutineLog::RoutineCategory::kSystem)
                  : "";
  const std::string network_routines =
      routine_log ? routine_log->GetContentsForCategory(
                        RoutineLog::RoutineCategory::kNetwork)
                  : "";

  // Fetch system data from TelemetryLog.
  const std::string system_log_contents =
      telemetry_log ? telemetry_log->GetContents() : "";

  std::vector<std::string> pieces;
  pieces.push_back(kSystemLogSectionHeader);
  if (!system_log_contents.empty()) {
    pieces.push_back(system_log_contents);
  }

  // Add the routine section for the system category.
  pieces.push_back(GetRoutineResultsString(system_routines));

  // Add networking category.
  pieces.push_back(kNetworkingLogSectionHeader);

  // Add the network info section.
  if (networking_log) {
    pieces.push_back(networking_log->GetNetworkInfo());
  }

  // Add the routine section for the network category.
  pieces.push_back(GetRoutineResultsString(network_routines));

  // Add the network events section.
  if (networking_log) {
    pieces.push_back(networking_log->GetNetworkEvents());
  }

  return base::WriteFile(file_path, base::JoinString(pieces, "\n"));
}

}  // namespace ash::diagnostics
