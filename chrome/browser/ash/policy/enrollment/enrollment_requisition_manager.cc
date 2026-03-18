// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "build/config/cuttlefish/buildflags.h"
#include "build/config/squid/buildflags.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

using ::ash::system::StatisticsProvider;

// static
const char EnrollmentRequisitionManager::kNoRequisition[] = "none";
const char EnrollmentRequisitionManager::kRemoraRequisition[] = "remora";
const char EnrollmentRequisitionManager::kSharkRequisition[] = "shark";
const char EnrollmentRequisitionManager::kDemoRequisition[] = "cros-demo-mode";
const char EnrollmentRequisitionManager::kCuttlefishRequisition[] =
    "cuttlefish";
const char EnrollmentRequisitionManager::kSquidRequisition[] = "squid";

// static
void EnrollmentRequisitionManager::Initialize(PrefService& local_state) {
  // OEM statistics are only loaded when OOBE is not completed.
  if (ash::StartupUtils::IsOobeCompleted())
    return;

  // Demo requisition may have been set in a prior enrollment attempt that was
  // interrupted.
  ash::DemoSetupController::ClearDemoRequisition(local_state);
  auto* provider = StatisticsProvider::GetInstance();
  const PrefService::Preference* pref =
      local_state.FindPreference(ash::prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    const std::optional<std::string_view> requisition =
        provider->GetMachineStatistic(ash::system::kOemDeviceRequisitionKey);

    if (requisition && !requisition->empty()) {
      local_state.SetString(ash::prefs::kDeviceEnrollmentRequisition,
                            requisition.value());
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition) {
        SetDeviceEnrollmentAutoStart(local_state);
      } else {
        const bool auto_start = StatisticsProvider::FlagValueToBool(
            provider->GetMachineFlag(ash::system::kOemIsEnterpriseManagedKey),
            /*default_value=*/false);
        local_state.SetBoolean(ash::prefs::kDeviceEnrollmentAutoStart,
                               auto_start);
        const bool can_exit = StatisticsProvider::FlagValueToBool(
            provider->GetMachineFlag(
                ash::system::kOemCanExitEnterpriseEnrollmentKey),
            /*default_value=*/false);
        local_state.SetBoolean(ash::prefs::kDeviceEnrollmentCanExit, can_exit);
      }
    }
  }
}

// static
std::string EnrollmentRequisitionManager::GetDeviceRequisition(
    const PrefService& local_state) {
  const PrefService::Preference* pref =
      local_state.FindPreference(ash::prefs::kDeviceEnrollmentRequisition);
  std::string requisition;
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    requisition = pref->GetValue()->GetString();

  if (requisition == kNoRequisition)
    requisition.clear();

  return requisition;
}

// static
void EnrollmentRequisitionManager::SetDeviceRequisition(
    PrefService& local_state,
    const std::string& requisition) {
  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "SetDeviceRequisition " << requisition;

  if (requisition.empty()) {
    local_state.ClearPref(ash::prefs::kDeviceEnrollmentRequisition);
    local_state.ClearPref(ash::prefs::kDeviceEnrollmentAutoStart);
    local_state.ClearPref(ash::prefs::kDeviceEnrollmentCanExit);
  } else {
    local_state.SetString(ash::prefs::kDeviceEnrollmentRequisition,
                          requisition);
    if (requisition == kNoRequisition) {
      local_state.ClearPref(ash::prefs::kDeviceEnrollmentAutoStart);
      local_state.ClearPref(ash::prefs::kDeviceEnrollmentCanExit);
    } else {
      SetDeviceEnrollmentAutoStart(local_state);
    }
  }
}

// static
bool EnrollmentRequisitionManager::IsRemoraRequisition(
    const PrefService& local_state) {
  return GetDeviceRequisition(local_state) == kRemoraRequisition;
}

// static
bool EnrollmentRequisitionManager::IsSharkRequisition(
    const PrefService& local_state) {
  return GetDeviceRequisition(local_state) == kSharkRequisition;
}

bool EnrollmentRequisitionManager::IsMeetDevice(
    const PrefService& local_state) {
#if BUILDFLAG(PLATFORM_CFM)
  return true;
#else
  return IsRemoraRequisition(local_state);
#endif  // BUILDFLAG(PLATFORM_CFM)
}

bool EnrollmentRequisitionManager::IsCuttlefishDevice() {
#if BUILDFLAG(PLATFORM_CUTTLEFISH)
  return true;
#else
  return false;
#endif  // BUILDFLAG(PLATFORM_CUTTLEFISH)
}

bool EnrollmentRequisitionManager::IsSquidDevice() {
#if BUILDFLAG(PLATFORM_SQUID)
  return true;
#else
  return false;
#endif  // BUILDFLAG(PLATFORM_SQUID)
}

// static
std::string EnrollmentRequisitionManager::GetSubOrganization(
    const PrefService& local_state) {
  const PrefService::Preference* pref =
      local_state.FindPreference(ash::prefs::kDeviceEnrollmentSubOrganization);
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    return pref->GetValue()->GetString();
  return std::string();
}

// static
void EnrollmentRequisitionManager::SetSubOrganization(
    PrefService& local_state,
    const std::string& sub_organization) {
  if (sub_organization.empty()) {
    local_state.ClearPref(ash::prefs::kDeviceEnrollmentSubOrganization);
  } else {
    local_state.SetString(ash::prefs::kDeviceEnrollmentSubOrganization,
                          sub_organization);
  }
}

// static
void EnrollmentRequisitionManager::SetDeviceEnrollmentAutoStart(
    PrefService& local_state) {
  local_state.SetBoolean(ash::prefs::kDeviceEnrollmentAutoStart, true);
  local_state.SetBoolean(ash::prefs::kDeviceEnrollmentCanExit, false);
}

// static
void EnrollmentRequisitionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(ash::prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterStringPref(ash::prefs::kDeviceEnrollmentSubOrganization,
                               std::string());
  registry->RegisterBooleanPref(ash::prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(ash::prefs::kDeviceEnrollmentCanExit, true);
  registry->RegisterStringPref(ash::prefs::kEnrollmentVersionOS, std::string());
  registry->RegisterStringPref(ash::prefs::kEnrollmentVersionBrowser,
                               std::string());
}

}  // namespace policy
