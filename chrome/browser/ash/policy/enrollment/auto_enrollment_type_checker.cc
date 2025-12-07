// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Returns true if this is an official build and the device has Chrome firmware.
static bool IsOfficialGoogleChrome() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  const std::optional<std::string_view> firmware_type =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey);
  return firmware_type != ash::system::kFirmwareTypeValueNonchrome;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// Returns true if this is an official Flex build.
static bool IsOfficialGoogleFlex() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  return ash::switches::IsRevenBranding();
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// Returns true if this is an official Google OS.
static bool IsOfficialGoogleOS() {
  return IsOfficialGoogleChrome() || IsOfficialGoogleFlex();
}

// Returns true if we are on an officially branded Flex and FRE is enabled
// on Flex.
static bool IsOfficialGoogleFlexAndFREOnFlexIsEnabled() {
  return IsOfficialGoogleFlex() &&
         // FRE on Flex is enabled unless explicitly disabled ("never" enabled).
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             ash::switches::kEnterpriseEnableForcedReEnrollmentOnFlex) !=
             AutoEnrollmentTypeChecker::kFlexForcedReEnrollmentNever;
}

}  // namespace

// Returns true if FRE state keys are supported.
bool AutoEnrollmentTypeChecker::AreFREStateKeysSupported() {
  // TODO(b/331677599): Return IsOfficialGoogleOS().
  return IsOfficialGoogleChrome() ||
         IsOfficialGoogleFlexAndFREOnFlexIsEnabled();
}

// static
bool AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination);
  if (command_line_mode == kUnifiedStateDeterminationAlways) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledViaAlwaysSwitch);
    return true;
  }
  if (command_line_mode == kUnifiedStateDeterminationNever) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kDisabledViaNeverSwitch);
    return false;
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                USDStatus::kDisabledOnUnbrandedBuild);
#else
  if (IsOfficialGoogleChrome()) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledOnOfficialGoogleChrome);
  } else if (IsOfficialGoogleFlex()) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledOnOfficialGoogleFlex);
  } else {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kDisabledOnNonChromeDevice);
  }
#endif

  // Official Google OSes support unified state determination.
  return IsOfficialGoogleOS();
}

// static
bool AutoEnrollmentTypeChecker::IsEnabled() {
  return IsUnifiedStateDeterminationEnabled();
}

}  // namespace policy
