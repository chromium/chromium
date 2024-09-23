// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features_parser.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "chrome/browser/metrics/chromeos_system_profile_provider.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/structured/recorder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/variations/hashing.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "ui/display/display.h"
#include "ui/events/event_utils.h"

using metrics::ChromeUserMetricsExtension;
using metrics::SampledProfile;
using metrics::SystemProfileProto;

namespace {

inline constexpr char kFeatureManagementLevelFlag[] =
    "feature-management-level";
inline constexpr char kFeatureManagementMaxLevelFlag[] =
    "feature-management-max-level";
inline constexpr char kFeatureManagementScopeFlag[] =
    "feature-management-scope";

void IncrementPrefValue(const char* path, int num_samples) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + num_samples);
}

}  // namespace

ChromeOSMetricsProvider::ChromeOSMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType service_type,
    ChromeOSSystemProfileProvider* cros_system_profile_provider)
    : cros_system_profile_provider_(cros_system_profile_provider) {
  DCHECK(cros_system_profile_provider_);
  if (service_type == metrics::MetricsLogUploader::UMA)
    profile_provider_ = std::make_unique<metrics::ProfileProvider>();
}

ChromeOSMetricsProvider::~ChromeOSMetricsProvider() = default;

// static
void ChromeOSMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityOtherUserCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityKernelCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilitySystemUncleanShutdownCount, 0);
}

// static
void ChromeOSMetricsProvider::LogCrash(const std::string& crash_type,
                                       int num_samples) {
  if (crash_type == "user") {
    IncrementPrefValue(prefs::kStabilityOtherUserCrashCount, num_samples);
  } else if (crash_type == "kernel") {
    IncrementPrefValue(prefs::kStabilityKernelCrashCount, num_samples);
  } else if (crash_type == "uncleanshutdown") {
    IncrementPrefValue(prefs::kStabilitySystemUncleanShutdownCount,
                       num_samples);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Unexpected Chrome OS crash type " << crash_type;
  }

  // Wake up metrics logs sending if necessary now that new
  // log data is available.
  g_browser_process->metrics_service()->OnApplicationNotIdle();
}

EnrollmentStatus ChromeOSMetricsProvider::GetEnrollmentStatus() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector)
    return EnrollmentStatus::kErrorGettingStatus;

  return connector->IsDeviceEnterpriseManaged() ? EnrollmentStatus::kManaged
                                                : EnrollmentStatus::kNonManaged;
}

void ChromeOSMetricsProvider::Init() {
  if (profile_provider_)
    profile_provider_->Init();
}

void ChromeOSMetricsProvider::OnDidCreateMetricsLog() {
  cros_system_profile_provider_->OnDidCreateMetricsLog();
  if (!arc::StabilityMetricsManager::Get()) {
    return;
  }
  // Not guaranteed to result in emitting hisotograms when called early on
  // browser startup.
  arc::StabilityMetricsManager::Get()->RecordMetricsToUMA();
  emitted_ = UpdateUserTypeUMA();
}

void ChromeOSMetricsProvider::OnRecordingEnabled() {
  if (profile_provider_)
    profile_provider_->OnRecordingEnabled();
}

void ChromeOSMetricsProvider::OnRecordingDisabled() {
  if (profile_provider_)
    profile_provider_->OnRecordingDisabled();
}

void ChromeOSMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  cros_system_profile_provider_->ProvideSystemProfileMetrics(
      system_profile_proto);
}

void ChromeOSMetricsProvider::ProvideAccessibilityMetrics() {
  bool is_spoken_feedback_enabled =
      ash::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSpokenFeedback.EveryReport",
                        is_spoken_feedback_enabled);
}

void ChromeOSMetricsProvider::ProvideSuggestedContentMetrics() {
  UMA_HISTOGRAM_BOOLEAN(
      "Apps.AppList.SuggestedContent.Enabled",
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::prefs::kSuggestedContentEnabled));
}

void ChromeOSMetricsProvider::ProvideMetrics(
    metrics::SystemProfileProto* system_profile_proto,
    bool should_include_arc_metrics) {
  metrics::SystemProfileProto::Stability* stability_proto =
      system_profile_proto->mutable_stability();
  PrefService* pref = g_browser_process->local_state();
  int count = pref->GetInteger(prefs::kStabilityOtherUserCrashCount);
  if (count) {
    stability_proto->set_other_user_crash_count(count);
    pref->SetInteger(prefs::kStabilityOtherUserCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilityKernelCrashCount);
  if (count) {
    stability_proto->set_kernel_crash_count(count);
    pref->SetInteger(prefs::kStabilityKernelCrashCount, 0);
  }

  count = pref->GetInteger(prefs::kStabilitySystemUncleanShutdownCount);
  if (count) {
    stability_proto->set_unclean_system_shutdown_count(count);
    pref->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 0);
  }

  // Use current enrollment status for initial stability logs, since it's not
  // likely to change between browser restarts.
  UMA_STABILITY_HISTOGRAM_ENUMERATION(
      "UMA.EnrollmentStatus", GetEnrollmentStatus(),
      // static_cast because we only have macros for stability histograms.
      static_cast<int>(EnrollmentStatus::kMaxValue) + 1);

  if (should_include_arc_metrics) {
    // Record ARC-related stability metrics that should be included in initial
    // stability logs and all regular UMA logs.
    arc::StabilityMetricsManager::Get()->RecordMetricsToUMA();
  }
}

void ChromeOSMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  ProvideMetrics(system_profile_proto, /*should_include_arc_metrics=*/true);
}

void ChromeOSMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProvideAccessibilityMetrics();
  ProvideSuggestedContentMetrics();
  ProvideMetrics(uma_proto->mutable_system_profile(),
                 /*should_include_arc_metrics=*/!emitted_);
  std::vector<SampledProfile> sampled_profiles;
  if (profile_provider_->GetSampledProfiles(&sampled_profiles)) {
    for (auto& profile : sampled_profiles) {
      uma_proto->add_sampled_profile()->Swap(&profile);
    }
  }
  arc::UpdateEnabledStateByUserTypeUMA();
  if (!emitted_) {
    UpdateUserTypeUMA();
  }
}

void ChromeOSMetricsProvider::ProvideCurrentSessionUKMData() {
  ukm::SourceId source_id = ukm::NoURLSourceId();
  EnrollmentStatus status = GetEnrollmentStatus();
  ukm::builders::ChromeOS_DeviceManagement(source_id)
      .SetEnrollmentStatus(static_cast<int64_t>(status))
      .Record(ukm::UkmRecorder::Get());
}

bool ChromeOSMetricsProvider::UpdateUserTypeUMA() {
  if (!user_manager::UserManager::IsInitialized()) {
    return false;
  }
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    return false;
  }
  user_manager::UserType user_type = primary_user->GetType();
  base::UmaHistogramEnumeration("UMA.PrimaryUserType", user_type);
  return true;
}

ChromeOSHistogramMetricsProvider::ChromeOSHistogramMetricsProvider() = default;

ChromeOSHistogramMetricsProvider::~ChromeOSHistogramMetricsProvider() = default;

bool ChromeOSHistogramMetricsProvider::ProvideHistograms() {
  // The scope type. Used in a histogram; do not modify existing types.
  // see histograms/enums.xml.
  enum {
    FEATURE_MANAGEMENT_REGULAR = 0,
    FEATURE_MANAGEMENT_SOFT_BRANDED = 1,
    FEATURE_MANAGEMENT_HARD_BRANDED = 2,
    kMaxValue = FEATURE_MANAGEMENT_HARD_BRANDED
  } scope_level;

  if (!base::CommandLine::InitializedForCurrentProcess()) {
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kFeatureManagementLevelFlag) ||
      !command_line->HasSwitch(kFeatureManagementMaxLevelFlag) ||
      !command_line->HasSwitch(kFeatureManagementScopeFlag)) {
    return false;
  }
  int feature_level = -1;
  int feature_max_level = -1;
  int scope_level_raw = -1;
  if (!base::StringToInt(
          command_line->GetSwitchValueASCII(kFeatureManagementLevelFlag),
          &feature_level) ||
      !base::StringToInt(
          command_line->GetSwitchValueASCII(kFeatureManagementMaxLevelFlag),
          &feature_max_level) ||
      !base::StringToInt(
          command_line->GetSwitchValueASCII(kFeatureManagementScopeFlag),
          &scope_level_raw)) {
    return false;
  }
  if (feature_level < 0 || feature_max_level < 0 || scope_level_raw < 0 ||
      feature_max_level < feature_level) {
    LOG(ERROR) << "Invalid FeatureLevel arguments: "
               << kFeatureManagementLevelFlag << " (" << feature_level
               << ") or " << kFeatureManagementMaxLevelFlag << " ("
               << feature_max_level << ") or " << kFeatureManagementScopeFlag
               << " (" << scope_level_raw << ")";
    return false;
  }
  if (feature_level == 0 && scope_level_raw == 0) {
    scope_level = FEATURE_MANAGEMENT_REGULAR;
  } else if (feature_level > 0 && scope_level_raw == 0) {
    scope_level = FEATURE_MANAGEMENT_SOFT_BRANDED;
  } else if (feature_level > 0 && scope_level_raw == 1) {
    scope_level = FEATURE_MANAGEMENT_HARD_BRANDED;
  } else {
    LOG(ERROR) << "Invalid ScopeLevel:" << kFeatureManagementLevelFlag << " ("
               << feature_level << ") or " << kFeatureManagementScopeFlag
               << " (" << scope_level_raw << ")";
    return false;
  }

  base::UmaHistogramExactLinear("Platform.Segmentation.FeatureLevel",
                                feature_level, feature_max_level + 1);
  base::UmaHistogramEnumeration("Platform.Segmentation.ScopeLevel",
                                scope_level);
  return true;
}
