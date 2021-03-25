// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/hash/md5.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features_parser.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/variations/hashing.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "ui/display/display.h"
#include "ui/events/event_utils.h"

using metrics::ChromeUserMetricsExtension;
using metrics::SampledProfile;
using metrics::SystemProfileProto;

namespace {

void IncrementPrefValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + 1);
}

// Called on a background thread to load hardware class information.
std::string GetFullHardwareClassOnBackgroundThread() {
  std::string full_hardware_class;
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      "hardware_class", &full_hardware_class);
  return full_hardware_class;
}

bool IsFeatureEnabled(
    const chromeos::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map,
    chromeos::multidevice_setup::mojom::Feature feature) {
  return feature_states_map.find(feature)->second ==
         chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

}  // namespace

ChromeOSMetricsProvider::ChromeOSMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType service_type)
    : cached_profile_(std::make_unique<metrics::CachedMetricsProfile>()),
      registered_user_count_at_log_initialization_(false),
      user_count_at_log_initialization_(0) {
  if (service_type == metrics::MetricsLogUploader::UMA)
    profile_provider_ = std::make_unique<metrics::ProfileProvider>();
}

ChromeOSMetricsProvider::~ChromeOSMetricsProvider() {
}

// static
void ChromeOSMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityOtherUserCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityKernelCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilitySystemUncleanShutdownCount, 0);
}

// static
void ChromeOSMetricsProvider::LogCrash(const std::string& crash_type) {
  if (crash_type == "user")
    IncrementPrefValue(prefs::kStabilityOtherUserCrashCount);
  else if (crash_type == "kernel")
    IncrementPrefValue(prefs::kStabilityKernelCrashCount);
  else if (crash_type == "uncleanshutdown")
    IncrementPrefValue(prefs::kStabilitySystemUncleanShutdownCount);
  else
    NOTREACHED() << "Unexpected Chrome OS crash type " << crash_type;

  // Wake up metrics logs sending if necessary now that new
  // log data is available.
  g_browser_process->metrics_service()->OnApplicationNotIdle();
}

EnrollmentStatus ChromeOSMetricsProvider::GetEnrollmentStatus() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return EnrollmentStatus::kErrorGettingStatus;

  return connector->IsEnterpriseManaged() ? EnrollmentStatus::kManaged
                                          : EnrollmentStatus::kNonManaged;
}

void ChromeOSMetricsProvider::Init() {
  if (profile_provider_)
    profile_provider_->Init();
}

void ChromeOSMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  base::RepeatingClosure barrier =
      base::BarrierClosure(2, std::move(done_callback));
  InitTaskGetFullHardwareClass(barrier);
  InitTaskGetArcFeatures(barrier);
}

void ChromeOSMetricsProvider::OnDidCreateMetricsLog() {
  registered_user_count_at_log_initialization_ = false;
  if (user_manager::UserManager::IsInitialized()) {
    registered_user_count_at_log_initialization_ = true;
    user_count_at_log_initialization_ =
        user_manager::UserManager::Get()->GetLoggedInUsers().size();
  }
}

void ChromeOSMetricsProvider::OnRecordingEnabled() {
  if (profile_provider_)
    profile_provider_->OnRecordingEnabled();
}

void ChromeOSMetricsProvider::OnRecordingDisabled() {
  if (profile_provider_)
    profile_provider_->OnRecordingDisabled();
}

void ChromeOSMetricsProvider::InitTaskGetFullHardwareClass(
    base::OnceClosure callback) {
  // Run the (potentially expensive) task in the background to avoid blocking
  // the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetFullHardwareClassOnBackgroundThread),
      base::BindOnce(&ChromeOSMetricsProvider::SetFullHardwareClass,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSMetricsProvider::InitTaskGetArcFeatures(
    base::OnceClosure callback) {
  arc::ArcFeaturesParser::GetArcFeatures(
      base::BindOnce(&ChromeOSMetricsProvider::OnArcFeaturesParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  WriteLinkedAndroidPhoneProto(system_profile_proto);
  UpdateMultiProfileUserCount(system_profile_proto);

  metrics::SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();
  hardware->set_full_hardware_class(full_hardware_class_);
  display::Display::TouchSupport has_touch =
      ui::GetInternalDisplayTouchSupport();
  if (has_touch == display::Display::TouchSupport::AVAILABLE)
    hardware->set_internal_display_supports_touch(true);
  else if (has_touch == display::Display::TouchSupport::UNAVAILABLE)
    hardware->set_internal_display_supports_touch(false);

  if (arc_release_) {
    metrics::SystemProfileProto::OS::Arc* arc =
        system_profile_proto->mutable_os()->mutable_arc();
    arc->set_release(*arc_release_);
  }
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
          chromeos::prefs::kSuggestedContentEnabled));
}

void ChromeOSMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
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

  // Record ARC-related stability metrics that should be included in initial
  // stability logs and all regular UMA logs.
  arc::StabilityMetricsManager::Get()->RecordMetricsToUMA();
}

void ChromeOSMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProvideAccessibilityMetrics();
  ProvideSuggestedContentMetrics();
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());
  std::vector<SampledProfile> sampled_profiles;
  if (profile_provider_->GetSampledProfiles(&sampled_profiles)) {
    for (auto& profile : sampled_profiles) {
      uma_proto->add_sampled_profile()->Swap(&profile);
    }
  }
  arc::UpdateEnabledStateByUserTypeUMA();
  UpdateUserTypeUMA();
}

void ChromeOSMetricsProvider::WriteLinkedAndroidPhoneProto(
    metrics::SystemProfileProto* system_profile_proto) {
  chromeos::multidevice_setup::MultiDeviceSetupClient* client =
      chromeos::multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
          cached_profile_->GetMetricsProfile());

  if (!client)
    return;

  const chromeos::multidevice_setup::MultiDeviceSetupClient::
      HostStatusWithDevice& host_status_with_device = client->GetHostStatus();
  if (host_status_with_device.first !=
      chromeos::multidevice_setup::mojom::HostStatus::kHostVerified) {
    return;
  }

  SystemProfileProto::LinkedAndroidPhoneData* linked_android_phone_data =
      system_profile_proto->mutable_linked_android_phone_data();
  const uint32_t hashed_name =
      variations::HashName(host_status_with_device.second->pii_free_name());
  linked_android_phone_data->set_phone_model_name_hash(hashed_name);

  const chromeos::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
      feature_states_map = client->GetFeatureStates();
  linked_android_phone_data->set_is_smartlock_enabled(IsFeatureEnabled(
      feature_states_map,
      chromeos::multidevice_setup::mojom::Feature::kSmartLock));
  linked_android_phone_data->set_is_instant_tethering_enabled(IsFeatureEnabled(
      feature_states_map,
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering));
  linked_android_phone_data->set_is_messages_enabled(
      IsFeatureEnabled(feature_states_map,
                       chromeos::multidevice_setup::mojom::Feature::kMessages));
}

void ChromeOSMetricsProvider::UpdateMultiProfileUserCount(
    metrics::SystemProfileProto* system_profile_proto) {
  if (user_manager::UserManager::IsInitialized()) {
    size_t user_count =
        user_manager::UserManager::Get()->GetLoggedInUsers().size();

    // We invalidate the user count if it changed while the log was open.
    if (registered_user_count_at_log_initialization_ &&
        user_count != user_count_at_log_initialization_) {
      user_count = 0;
    }

    system_profile_proto->set_multi_profile_user_count(user_count);
  }
}

void ChromeOSMetricsProvider::SetFullHardwareClass(
    base::OnceClosure callback,
    std::string full_hardware_class) {
  full_hardware_class_ = full_hardware_class;
  std::move(callback).Run();
}

void ChromeOSMetricsProvider::OnArcFeaturesParsed(
    base::OnceClosure callback,
    base::Optional<arc::ArcFeatures> features) {
  base::ScopedClosureRunner runner(std::move(callback));
  if (!features) {
    LOG(WARNING) << "ArcFeatures not available on this build";
    return;
  }
  arc_release_ = features->build_props.at("ro.build.version.release");
}

void ChromeOSMetricsProvider::UpdateUserTypeUMA() {
  if (!user_manager::UserManager::IsInitialized())
    return;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return;
  user_manager::UserType user_type = primary_user->GetType();
  base::UmaHistogramEnumeration("UMA.PrimaryUserType", user_type,
                                user_manager::UserType::NUM_USER_TYPES);
}
