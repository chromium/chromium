// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CONNECTIVITY_PROBLEM_FORMATTER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CONNECTIVITY_PROBLEM_FORMATTER_H_

#include <string>
#include <vector>

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::diagnostics {

// Formats GoogleServicesConnectivityProblem entries into structured
// plain text for the session log.
//
// Each problem produces a multi-line block:
//   hostname:                       (omitted if empty)
//     Status: FAIL - ConnectionFailure
//     Proxy: proxy:8080           (optional)
//     Start: 2/19/26, 10:29 AM   (optional)
//     End: 2/19/26, 10:29 AM     (optional)
//     Error: Couldn't connect
//     Resolution: Check firewall  (optional)
//
// Blocks are separated by an empty line.
// Returns empty string if `problems` is empty.
std::string FormatConnectivityProblems(
    const std::vector<chromeos::network_diagnostics::mojom::
                          GoogleServicesConnectivityProblemPtr>& problems);

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_CONNECTIVITY_PROBLEM_FORMATTER_H_
