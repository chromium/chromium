// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Returns true if this is an official build and the device has Chrome firmware.
bool IsGoogleBrandedChrome() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  const absl::optional<base::StringPiece> firmware_type =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey);
  return firmware_type != ash::system::kFirmwareTypeValueNonchrome;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::string FRERequirementToString(
    AutoEnrollmentTypeChecker::FRERequirement requirement) {
  using FRERequirement = AutoEnrollmentTypeChecker::FRERequirement;
  switch (requirement) {
    case FRERequirement::kDisabled:
      return "Forced Re-Enrollment disabled via command line.";
    case FRERequirement::kRequired:
      return "Forced Re-Enrollment required.";
    case FRERequirement::kNotRequired:
      return "Forced Re-Enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Forced Re-Enrollment explicitly required.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Forced Re-Enrollment explicitly not required.";
  }
}

// Kill switch config request parameters.
constexpr net::NetworkTrafficAnnotationTag kKSConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "unified_state_determination_kill_switch",
        R"(
            semantics {
              sender: "Unified State Determination"
              description:
                "Communication with the backend used to check whether "
                "unified state determination should be enabled."
              trigger: "Open device for the first time, powerwash the device."
              data: "A simple GET HTTP request without user data."
              destination: GOOGLE_OWNED_SERVICE
              internal {
                contacts {
                  email: "sergiyb@google.com"
                }
                contacts {
                  email: "chromeos-commercial-remote-management@google.com"
                }
              }
              user_data {
                type: NONE
              }
              last_reviewed: "2023-05-16"
            }
            policy {
              cookies_allowed: NO
              setting: "This feature cannot be controlled by Chrome settings."
              chrome_policy {}
            })");
constexpr char kKSConfigUrl[] =
    "https://www.gstatic.com/chromeos-usd-experiment/v1.json";
constexpr base::TimeDelta kKSConfigFetchTimeout = base::Seconds(1);
constexpr int kKSConfigFetchTries = 4;
constexpr size_t kKSConfigMaxSize = 1024;  // 1KB
constexpr char kKSConfigFetchMethod[] = "GET";
constexpr char kKSConfigDisableUpToVersionKey[] = "disable_up_to_version";
constexpr int kUMAKSFetchNumTriesMinValue = 1;
constexpr int kUMAKSFetchNumTriesExclusiveMaxValue = 51;
constexpr int kUMAKSFetchNumTriesBuckets =
    kUMAKSFetchNumTriesExclusiveMaxValue - kUMAKSFetchNumTriesMinValue;

// This value represents current version of the code. After we have enabled kill
// switch for a particular version, we can increment it after fixing the logic.
// The devices running new code will not be affected by kill switch and we can
// test our fixes.
// TODO(b/265923216): Change to 1 to launch unified state determination.
const int kCodeVersion = 0;

// When set to true, unified state determination is disabled.
absl::optional<bool> g_unified_state_determination_kill_switch;

void ReportKillSwitchFetchTries(int tries) {
  base::UmaHistogramCustomCounts(kUMAStateDeterminationKillSwitchFetchNumTries,
                                 tries, kUMAKSFetchNumTriesMinValue,
                                 kUMAKSFetchNumTriesExclusiveMaxValue,
                                 kUMAKSFetchNumTriesBuckets);
}

void ParseKSConfig(base::OnceClosure init_callback,
                   const std::string& response) {
  absl::optional<base::Value> config = base::JSONReader::Read(response);
  if (!config || !config->is_dict()) {
    LOG(ERROR) << "Kill switch config is not valid JSON or not a dict";
    std::move(init_callback).Run();
    return;
  }

  absl::optional<int> disable_up_to_version =
      config->GetDict().FindInt(kKSConfigDisableUpToVersionKey);
  if (!disable_up_to_version) {
    LOG(ERROR) << "Kill switch config is missing disable_up_to_version key or "
                  "it is not an int";
    std::move(init_callback).Run();
    return;
  }

  g_unified_state_determination_kill_switch =
      kCodeVersion <= disable_up_to_version;
  std::move(init_callback).Run();
}

void FetchKSConfig(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::OnceClosure init_callback,
    int tries_left,
    std::unique_ptr<network::SimpleURLLoader> loader = nullptr,
    std::unique_ptr<std::string> response = nullptr) {
  if (loader) {
    base::UmaHistogramSparse(
        kUMAStateDeterminationKillSwitchFetchNetworkErrorCode,
        -loader->NetError());
  }

  if (!response && tries_left) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GURL(kKSConfigUrl);
    request->method = kKSConfigFetchMethod;
    request->load_flags = net::LOAD_DISABLE_CACHE;
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    VLOG(1) << "Sending kill switch config request to " << request->url;
    loader = network::SimpleURLLoader::Create(std::move(request),
                                              kKSConfigTrafficAnnotation);
    loader->SetTimeoutDuration(kKSConfigFetchTimeout);
    // Use the raw pointer to avoid calling on empty `loader` after std::move.
    network::SimpleURLLoader* loader_ptr = loader.get();
    loader_ptr->DownloadToString(
        loader_factory.get(),
        base::BindOnce(FetchKSConfig, loader_factory, std::move(init_callback),
                       tries_left - 1, std::move(loader)),
        kKSConfigMaxSize);
    return;
  }

  // On any errors, assume kill switch is enabled and fallback to old logic.
  g_unified_state_determination_kill_switch = true;
  if (!response) {
    LOG(ERROR) << "Kill switch config request failed with code "
               << loader->NetError();
    ReportKillSwitchFetchTries(kKSConfigFetchTries);
    std::move(init_callback).Run();
    return;
  }

  VLOG(1) << "Received kill switch config response after "
          << (kKSConfigFetchTries - tries_left) << " tries: " << *response;
  ReportKillSwitchFetchTries(kKSConfigFetchTries - tries_left);
  ParseKSConfig(std::move(init_callback), *response);
}

bool IsUnifiedStateDeterminationDisabledByKillSwitch() {
  // If AutoEnrollmentTypeChecker is not initialized, assume the kill switch is
  // enabled. This is for legacy code that doesn't know about unified state
  // determination. New code should wait for init to complete.
  return g_unified_state_determination_kill_switch.value_or(true);
}

}  // namespace

// static
void AutoEnrollmentTypeChecker::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::OnceClosure init_callback) {
  FetchKSConfig(loader_factory, std::move(init_callback), kKSConfigFetchTries);
}

// static
bool AutoEnrollmentTypeChecker::Initialized() {
  return g_unified_state_determination_kill_switch.has_value();
}

// static
bool AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination);
  if (command_line_mode == kUnifiedStateDeterminationAlways) {
    return true;
  }
  if (command_line_mode == kUnifiedStateDeterminationNever) {
    return false;
  }
  if (IsUnifiedStateDeterminationDisabledByKillSwitch()) {
    return false;
  }

  // Non-Google-branded Chrome indicates that we're running on non-Chrome
  // hardware. In that environment, it doesn't make sense to enable state
  // determination as state key generation is likely to fail.
  return IsGoogleBrandedChrome();
}

// static
bool AutoEnrollmentTypeChecker::IsFREEnabled() {
  // To support legacy code that does not support unified state determination
  // yet, we pretend FRE is explicitly enabled, when unified state determination
  // is enabled. For example, this enables state keys to be uploaded with the
  // policy fetches.
  // TODO(b/265923216): Migrate legacy code to support unified state
  // determination.
  if (IsUnifiedStateDeterminationEnabled()) {
    return true;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways) {
    return true;
  }
  if (command_line_mode.empty() ||
      command_line_mode == kForcedReEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kForcedReEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Forced Re-Enrollment mode: " << command_line_mode
             << ".";
  return false;
}

// static
bool AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnableInitialEnrollment))
    return IsGoogleBrandedChrome();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment);
  if (command_line_mode == kInitialEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kInitialEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kInitialEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Initial Enrollment mode: " << command_line_mode << ".";
  return false;
}

// static
bool AutoEnrollmentTypeChecker::IsEnabled() {
  return IsFREEnabled() || IsInitialEnrollmentEnabled();
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
    ash::system::StatisticsProvider* statistics_provider) {
  // To support legacy code that does not support unified state determination
  // yet, we pretend FRE is explicitly enabled, when unified state determination
  // is enabled. For example, this disables powerwash and TPM firmware updates
  // during OOBE (since admin could have forbidden both).
  // TODO(b/265923216): Migrate legacy code to support unified state
  // determination.
  if (IsUnifiedStateDeterminationEnabled()) {
    // Flex devices do not support FRE.
    if (ash::switches::IsRevenBranding()) {
      return FRERequirement::kRequired;
    }
    return FRERequirement::kExplicitlyRequired;
  }

  const absl::optional<base::StringPiece> check_enrollment_value =
      statistics_provider->GetMachineStatistic(
          ash::system::kCheckEnrollmentKey);

  if (check_enrollment_value) {
    if (check_enrollment_value == "0") {
      return FRERequirement::kExplicitlyNotRequired;
    }
    if (check_enrollment_value == "1") {
      return FRERequirement::kExplicitlyRequired;
    }

    LOG(ERROR) << "Unexpected value for " << ash::system::kCheckEnrollmentKey
               << ": " << check_enrollment_value.value();
    LOG(WARNING) << "Forcing auto enrollment check.";
    return FRERequirement::kExplicitlyRequired;
  }

  // FRE fails on reven, do not force FRE check.
  if (ash::switches::IsRevenBranding() &&
      statistics_provider->GetVpdStatus() !=
          ash::system::StatisticsProvider::VpdStatus::kValid) {
    LOG(WARNING) << "Re-enrollment is not forced on reven device";
    return FRERequirement::kRequired;
  }

  // The FRE flag is not found. If VPD is in valid state, do not require FRE
  // check if the device was never owned. If VPD is broken, continue with FRE
  // check.
  switch (statistics_provider->GetVpdStatus()) {
    // If RO_VPD is broken, state keys are not available and FRE check
    // cannot start. To not to get stuck with forced re-enrollment, do not
    // enforce it and let users cancel in case of permanent error.
    case ash::system::StatisticsProvider::VpdStatus::kInvalid:
      // Both RO and RW VPDs are broken and state keys are not available.
      // Require re-enrollment but do not force it.
      LOG(WARNING) << "RO_VPD and RW_VPD are broken.";
      return FRERequirement::kRequired;
    case ash::system::StatisticsProvider::VpdStatus::kRoInvalid:
      // RO_VPD is broken, but RW_VPD is valid. `kActivateDateKey` indicating
      // ownership is available and trustworthy. Proceed with with ownership
      // check and require  re-enrollment if the device was owned.
      LOG(WARNING) << "RO_VPD is broken. Proceeding with ownership check.";
      [[fallthrough]];
    case ash::system::StatisticsProvider::VpdStatus::kValid:
      if (!statistics_provider->GetMachineStatistic(
              ash::system::kActivateDateKey)) {
        // The device has never been activated (enterprise enrolled or
        // consumer-owned) so doing a FRE check is not necessary.
        return FRERequirement::kNotRequired;
      }
      return FRERequirement::kRequired;
    case ash::system::StatisticsProvider::VpdStatus::kRwInvalid:
      // VPD is in invalid state and FRE flag cannot be assessed. Force FRE
      // check to prevent enrollment escapes.
      LOG(ERROR) << "VPD could not be read, forcing auto-enrollment check.";
      return FRERequirement::kExplicitlyRequired;
    case ash::system::StatisticsProvider::VpdStatus::kUnknown:
      NOTREACHED() << "VPD status is unknown";
      return FRERequirement::kRequired;
  }
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirement(
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
  // Skip FRE check if it is not enabled by command-line switch.
  if (!IsFREEnabled()) {
    LOG(WARNING) << "FRE disabled.";
    return FRERequirement::kDisabled;
  }

  // Skip FRE check if modulus configuration is not present.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentModulusLimit)) {
    LOG(WARNING) << "FRE disabled through command line (config).";
    return FRERequirement::kNotRequired;
  }

  // The FWMP flag DEVELOPER_DISABLE_BOOT indicates that FRE was configured
  // in the previous OOBE. We need to force FRE checks to prevent enrollment
  // escapes, see b/268267865.
  if (dev_disable_boot) {
    return FRERequirement::kExplicitlyRequired;
  }

  return AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
      statistics_provider);
}

// static
AutoEnrollmentTypeChecker::InitialStateDeterminationRequirement
AutoEnrollmentTypeChecker::GetInitialStateDeterminationRequirement(
    bool is_system_clock_synchronized,
    ash::system::StatisticsProvider* statistics_provider) {
  // Skip Initial State Determination if it is not enabled according to
  // command-line switch.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return InitialStateDeterminationRequirement::kDisabled;
  }
  const ash::system::FactoryPingEmbargoState embargo_state =
      ash::system::GetEnterpriseManagementPingEmbargoState(statistics_provider);
  const absl::optional<base::StringPiece> serial_number =
      statistics_provider->GetMachineID();
  if (!serial_number || serial_number->empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing serial number.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  const absl::optional<base::StringPiece> rlz_brand_code =
      statistics_provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);
  if (!rlz_brand_code || rlz_brand_code->empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing brand code.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  switch (embargo_state) {
    case ash::system::FactoryPingEmbargoState::kMissingOrMalformed:
      LOG(WARNING) << "Initial State Determination required due to missing "
                      "embargo state.";
      return InitialStateDeterminationRequirement::kRequired;
    case ash::system::FactoryPingEmbargoState::kPassed:
      LOG(WARNING) << "Initial State Determination required due to passed "
                      "embargo state.";
      return InitialStateDeterminationRequirement::kRequired;
    case ash::system::FactoryPingEmbargoState::kNotPassed:
      if (!is_system_clock_synchronized) {
        LOG(WARNING) << "Cannot decide Initial State Determination due to out "
                        "of sync clock.";
        return InitialStateDeterminationRequirement::
            kUnknownDueToMissingSystemClockSync;
      }
      LOG(WARNING) << "Skip Initial State Determination because the device is "
                      "in the embargo period.";
      return InitialStateDeterminationRequirement::kNotRequired;
    case ash::system::FactoryPingEmbargoState::kInvalid:
      if (!is_system_clock_synchronized) {
        LOG(WARNING) << "Cannot decide Initial State Determination due to out "
                        "of sync clock.";
        return InitialStateDeterminationRequirement::
            kUnknownDueToMissingSystemClockSync;
      }
      LOG(WARNING) << "Skip Initial State Determination due to invalid embargo date.";
      return InitialStateDeterminationRequirement::kNotRequired;
  }
}

// static
AutoEnrollmentTypeChecker::CheckType
AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
    bool is_system_clock_synchronized,
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
  // The only user of this function is AutoEnrollmentController and it should
  // not be calling it when unified state determination is enabled. Instead, we
  // fake explicitly forced re-enrollment to prevent users from skipping it.
  DCHECK(!IsUnifiedStateDeterminationEnabled());

  // Skip everything if neither FRE nor Initial Enrollment are enabled.
  if (!IsEnabled()) {
    LOG(WARNING) << "Auto-enrollment disabled.";
    return CheckType::kNone;
  }

  // Skip everything if GAIA is disabled.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kDisableGaiaServices)) {
    LOG(WARNING) << "Auto-enrollment disabled: command line (gaia).";
    return CheckType::kNone;
  }

  // Determine whether to do an FRE check or an initial state determination.
  // FRE has precedence since managed devices must go through an FRE check.
  const FRERequirement fre_requirement =
      GetFRERequirement(statistics_provider, dev_disable_boot);
  LOG(WARNING) << FRERequirementToString(fre_requirement);

  switch (fre_requirement) {
    case FRERequirement::kDisabled:
    case FRERequirement::kNotRequired:
      // Fall to the initial state determination check.
      break;
    case FRERequirement::kExplicitlyNotRequired:
      // Force initial determination check even if explicitly not required.
      // TODO(igorcov): b/238592446 Return CheckType::kNone when that gets
      // fixed.
      break;
    case FRERequirement::kExplicitlyRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentExplicitlyRequired;
    case FRERequirement::kRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentImplicitlyRequired;
  }

  // FRE is not required. Check whether an initial state determination should be
  // done.
  switch (GetInitialStateDeterminationRequirement(is_system_clock_synchronized,
                                                  statistics_provider)) {
    case InitialStateDeterminationRequirement::kDisabled:
    case InitialStateDeterminationRequirement::kNotRequired:
      return CheckType::kNone;
    case InitialStateDeterminationRequirement::
        kUnknownDueToMissingSystemClockSync:
      return CheckType::kUnknownDueToMissingSystemClockSync;
    case InitialStateDeterminationRequirement::kRequired:
      LOG(WARNING) << "Proceeding with Initial State Determination.";
      return CheckType::kInitialStateDetermination;
  }

  // Neither FRE nor initial state determination checks are needed.
  return CheckType::kNone;
}

// static
void AutoEnrollmentTypeChecker::
    SetUnifiedStateDeterminationKillSwitchForTesting(bool enabled) {
  g_unified_state_determination_kill_switch = enabled;
}

// static
void AutoEnrollmentTypeChecker::
    ClearUnifiedStateDeterminationKillSwitchForTesting() {
  g_unified_state_determination_kill_switch.reset();
}

// static
bool AutoEnrollmentTypeChecker::
    IsUnifiedStateDeterminationDisabledByKillSwitchForTesting() {
  return IsUnifiedStateDeterminationDisabledByKillSwitch();
}

}  // namespace policy
