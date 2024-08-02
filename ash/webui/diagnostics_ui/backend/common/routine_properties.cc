// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/common/routine_properties.h"

namespace ash::diagnostics {

namespace healthd = cros_healthd::mojom;

const RoutineProperties kRoutineProperties[] = {
    {mojom::RoutineType::kBatteryCharge, "BatteryChargeResult",
     /*duration_seconds=*/30, healthd::DiagnosticRoutineEnum::kBatteryCharge},
    {mojom::RoutineType::kBatteryDischarge, "BatteryDischargeResult",
     /*duration_seconds=*/30,
     healthd::DiagnosticRoutineEnum::kBatteryDischarge},
    {mojom::RoutineType::kCpuCache, "CpuCacheResult",
     /*duration_seconds=*/60, healthd::DiagnosticRoutineEnum::kCpuCache},
    {mojom::RoutineType::kCpuStress, "CpuStressResult",
     /*duration_seconds=*/60, healthd::DiagnosticRoutineEnum::kCpuStress},
    {mojom::RoutineType::kCpuFloatingPoint, "CpuFloatingPointResult",
     /*duration_seconds=*/60,
     healthd::DiagnosticRoutineEnum::kFloatingPointAccuracy},
    {mojom::RoutineType::kCpuPrime, "CpuPrimeResult",
     /*duration_seconds=*/60, healthd::DiagnosticRoutineEnum::kPrimeSearch},
    {mojom::RoutineType::kMemory, "MemoryResult",
     /*duration_seconds=*/1000, healthd::DiagnosticRoutineEnum::kMemory},
    {mojom::RoutineType::kLanConnectivity, "LanConnectivityResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kLanConnectivity},
    {mojom::RoutineType::kSignalStrength, "SignalStrengthResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kSignalStrength},
    {mojom::RoutineType::kGatewayCanBePinged, "GatewayCanBePingedResult",
     /*duration_seconds=*/1,
     healthd::DiagnosticRoutineEnum::kGatewayCanBePinged},
    {mojom::RoutineType::kHasSecureWiFiConnection,
     "HasSecureWiFiConnectionResult",
     /*duration_seconds=*/1,
     healthd::DiagnosticRoutineEnum::kHasSecureWiFiConnection},
    {mojom::RoutineType::kDnsResolverPresent, "DnsResolverPresentResult",
     /*duration_seconds=*/1,
     healthd::DiagnosticRoutineEnum::kDnsResolverPresent},
    {mojom::RoutineType::kDnsLatency, "DnsLatencyResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kDnsLatency},
    {mojom::RoutineType::kDnsResolution, "DnsResolutionResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kDnsResolution},
    {mojom::RoutineType::kCaptivePortal, "CaptivePortalResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kCaptivePortal},
    {mojom::RoutineType::kHttpFirewall, "HttpFirewallResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kHttpFirewall},
    {mojom::RoutineType::kHttpsFirewall, "HttpsFirewallResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kHttpsFirewall},
    {mojom::RoutineType::kHttpsLatency, "HttpsLatencyResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kHttpsLatency},
    {mojom::RoutineType::kArcHttp, "ArcHttpResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kArcHttp},
    {mojom::RoutineType::kArcPing, "ArcPingResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kArcPing},
    {mojom::RoutineType::kArcDnsResolution, "ArcDnsResolutionResult",
     /*duration_seconds=*/1, healthd::DiagnosticRoutineEnum::kArcDnsResolution},
};

const size_t kRoutinePropertiesLength = std::size(kRoutineProperties);

static_assert(kRoutinePropertiesLength ==
                  static_cast<size_t>(mojom::RoutineType::kMaxValue) + 1,
              "Mismatch between routine properties and RoutineType enum");

std::string GetRoutineMetricName(mojom::RoutineType routine_type) {
  return GetRoutineProperties(routine_type).metric_name;
}

uint32_t GetExpectedRoutineDurationInSeconds(mojom::RoutineType routine_type) {
  return GetRoutineProperties(routine_type).duration_seconds;
}

const RoutineProperties& GetRoutineProperties(mojom::RoutineType routine_type) {
  return kRoutineProperties[static_cast<size_t>(routine_type)];
}

}  // namespace ash::diagnostics
