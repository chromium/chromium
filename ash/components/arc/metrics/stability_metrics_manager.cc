// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/metrics/stability_metrics_manager.h"

#include <ostream>

#include "ash/components/arc/arc_prefs.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace arc {

namespace {

constexpr char kArcEnabledStateKey[] = "enabled_state";
constexpr char kArcNativeBridgeTypeKey[] = "native_bridge_type";

StabilityMetricsManager* g_stability_metrics_manager = nullptr;

}  // namespace

// static
void StabilityMetricsManager::Initialize(PrefService* local_state) {
  DCHECK(!g_stability_metrics_manager);
  g_stability_metrics_manager = new StabilityMetricsManager(local_state);
}

// static
void StabilityMetricsManager::Shutdown() {
  DCHECK(g_stability_metrics_manager);
  delete g_stability_metrics_manager;
  g_stability_metrics_manager = nullptr;
}

// static
StabilityMetricsManager* StabilityMetricsManager::Get() {
  return g_stability_metrics_manager;
}

StabilityMetricsManager::StabilityMetricsManager(PrefService* local_state)
    : local_state_(local_state) {}

StabilityMetricsManager::~StabilityMetricsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StabilityMetricsManager::RecordMetricsToUMA() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FindPreference(prefs::kStabilityMetrics) should never return null, but
  // since this may be called early on browser startup, be paranoid here to
  // prevent going into a crash loop.
  if (const auto* pref = local_state_->FindPreference(prefs::kStabilityMetrics);
      !pref || pref->GetType() != base::Value::Type::DICT) {
    NOTREACHED() << "Local state unavailable, not recording stability metrics.";
  }

  const std::optional<bool> enabled_state = GetArcEnabledState();
  if (enabled_state)
    UMA_STABILITY_HISTOGRAM_ENUMERATION("Arc.State", *enabled_state ? 1 : 0, 2);

  const std::optional<NativeBridgeType> native_bridge_type =
      GetArcNativeBridgeType();
  if (native_bridge_type) {
    UMA_STABILITY_HISTOGRAM_ENUMERATION(
        "Arc.NativeBridge", *native_bridge_type,
        static_cast<int>(NativeBridgeType::kMaxValue) + 1);
  }
}

void StabilityMetricsManager::ResetMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->SetDict(prefs::kStabilityMetrics, base::Value::Dict());
}

std::optional<bool> StabilityMetricsManager::GetArcEnabledState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kStabilityMetrics);
  return dict.FindBool(kArcEnabledStateKey);
}

void StabilityMetricsManager::SetArcEnabledState(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(local_state_, prefs::kStabilityMetrics);
  update->Set(kArcEnabledStateKey, enabled);
}

std::optional<NativeBridgeType>
StabilityMetricsManager::GetArcNativeBridgeType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kStabilityMetrics);
  std::optional<int> native_bridge_type = dict.FindInt(kArcNativeBridgeTypeKey);
  if (native_bridge_type) {
    return std::make_optional(
        static_cast<NativeBridgeType>(*native_bridge_type));
  }
  return std::nullopt;
}

void StabilityMetricsManager::SetArcNativeBridgeType(
    NativeBridgeType native_bridge_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(local_state_, prefs::kStabilityMetrics);
  update->Set(kArcNativeBridgeTypeKey, static_cast<int>(native_bridge_type));
}

}  // namespace arc
