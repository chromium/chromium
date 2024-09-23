// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

#include <string_view>

#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "build/config/cuttlefish/buildflags.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
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

// static
void EnrollmentRequisitionManager::Initialize() {
  // OEM statistics are only loaded when OOBE is not completed.
  if (ash::StartupUtils::IsOobeCompleted())
    return;

  // Demo requisition may have been set in a prior enrollment attempt that was
  // interrupted.
  ash::DemoSetupController::ClearDemoRequisition();
  auto* local_state = g_browser_process->local_state();
  auto* provider = StatisticsProvider::GetInstance();
  const PrefService::Preference* pref =
      local_state->FindPreference(prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    const std::optional<std::string_view> requisition =
        provider->GetMachineStatistic(ash::system::kOemDeviceRequisitionKey);

    if (requisition && !requisition->empty()) {
      local_state->SetString(prefs::kDeviceEnrollmentRequisition,
                             requisition.value());
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition) {
        SetDeviceEnrollmentAutoStart();
      } else {
        const bool auto_start = StatisticsProvider::FlagValueToBool(
            provider->GetMachineFlag(ash::system::kOemIsEnterpriseManagedKey),
            /*default_value=*/false);
        local_state->SetBoolean(prefs::kDeviceEnrollmentAutoStart, auto_start);
        const bool can_exit = StatisticsProvider::FlagValueToBool(
            provider->GetMachineFlag(
                ash::system::kOemCanExitEnterpriseEnrollmentKey),
            /*default_value=*/false);
        local_state->SetBoolean(prefs::kDeviceEnrollmentCanExit, can_exit);
      }
    }
  }
}

// static
std::string EnrollmentRequisitionManager::GetDeviceRequisition() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDeviceEnrollmentRequisition);
  std::string requisition;
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    requisition = pref->GetValue()->GetString();

  if (requisition == kNoRequisition)
    requisition.clear();

  return requisition;
}

// static
void EnrollmentRequisitionManager::SetDeviceRequisition(
    const std::string& requisition) {
  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "SetDeviceRequisition " << requisition;

  auto* local_state = g_browser_process->local_state();
  if (requisition.empty()) {
    local_state->ClearPref(prefs::kDeviceEnrollmentRequisition);
    local_state->ClearPref(prefs::kDeviceEnrollmentAutoStart);
    local_state->ClearPref(prefs::kDeviceEnrollmentCanExit);
  } else {
    local_state->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
    if (requisition == kNoRequisition) {
      local_state->ClearPref(prefs::kDeviceEnrollmentAutoStart);
      local_state->ClearPref(prefs::kDeviceEnrollmentCanExit);
    } else {
      SetDeviceEnrollmentAutoStart();
    }
  }
}

// static
bool EnrollmentRequisitionManager::IsRemoraRequisition() {
  return GetDeviceRequisition() == kRemoraRequisition;
}

// static
bool EnrollmentRequisitionManager::IsSharkRequisition() {
  return GetDeviceRequisition() == kSharkRequisition;
}

bool EnrollmentRequisitionManager::IsMeetDevice() {
#if BUILDFLAG(PLATFORM_CFM)
  return true;
#else
  return IsRemoraRequisition();
#endif  // BUILDFLAG(PLATFORM_CFM)
}

bool EnrollmentRequisitionManager::IsCuttlefishDevice() {
#if BUILDFLAG(PLATFORM_CUTTLEFISH)
  return true;
#else
  return false;
#endif  // BUILDFLAG(PLATFORM_CUTTLEFISH)
}

// static
std::string EnrollmentRequisitionManager::GetSubOrganization() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDeviceEnrollmentSubOrganization);
  if (!pref->IsDefaultValue() && pref->GetValue()->is_string())
    return pref->GetValue()->GetString();
  return std::string();
}

// static
void EnrollmentRequisitionManager::SetSubOrganization(
    const std::string& sub_organization) {
  if (sub_organization.empty())
    g_browser_process->local_state()->ClearPref(
        prefs::kDeviceEnrollmentSubOrganization);
  else
    g_browser_process->local_state()->SetString(
        prefs::kDeviceEnrollmentSubOrganization, sub_organization);
}

// static
void EnrollmentRequisitionManager::SetDeviceEnrollmentAutoStart() {
  g_browser_process->local_state()->SetBoolean(
      prefs::kDeviceEnrollmentAutoStart, true);
  g_browser_process->local_state()->SetBoolean(prefs::kDeviceEnrollmentCanExit,
                                               false);
}

// static
void EnrollmentRequisitionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterStringPref(prefs::kDeviceEnrollmentSubOrganization,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
  registry->RegisterStringPref(prefs::kEnrollmentVersionOS, std::string());
  registry->RegisterStringPref(prefs::kEnrollmentVersionBrowser, std::string());
}

}  // namespace policy
