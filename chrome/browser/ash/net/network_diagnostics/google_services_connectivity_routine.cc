// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

}  // namespace

GoogleServicesConnectivityRoutine::GoogleServicesConnectivityRoutine(
    mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
}

GoogleServicesConnectivityRoutine::~GoogleServicesConnectivityRoutine() =
    default;

mojom::RoutineType GoogleServicesConnectivityRoutine::Type() {
  return mojom::RoutineType::kGoogleServicesConnectivity;
}

bool GoogleServicesConnectivityRoutine::CanRun() {
  return base::FeatureList::IsEnabled(
      ash::features::kGoogleServicesConnectivityRoutine);
}

void GoogleServicesConnectivityRoutine::Run() {
  CHECK(CanRun());
  // TODO(crbug.com/463098734): add implementation.
  AnalyzeResultsAndExecuteCallback();
}

void GoogleServicesConnectivityRoutine::AnalyzeResultsAndExecuteCallback() {
  set_verdict(problems_.empty() ? mojom::RoutineVerdict::kNoProblem
                                : mojom::RoutineVerdict::kProblem);

  set_problems(mojom::RoutineProblems::NewGoogleServicesConnectivityProblems(
      std::move(problems_)));
  ExecuteCallback();
}

}  // namespace ash::network_diagnostics
