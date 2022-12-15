// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_system_profile_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/metrics/structured/recorder.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/display/display.h"
#include "ui/events/event_utils.h"

namespace {

// Called on a background thread to load cellular device variant
// using ConfigFS.
std::string GetCellularDeviceVariantOnBackgroundThread() {
  constexpr char kFirmwareVariantPath[] =
      "/run/chromeos-config/v1/modem/firmware-variant";
  std::string cellular_device_variant;
  const base::FilePath modem_path = base::FilePath(kFirmwareVariantPath);
  if (base::PathExists(modem_path)) {
    base::ReadFileToString(modem_path, &cellular_device_variant);
  }
  VLOG(1) << "cellular_device_variant: " << cellular_device_variant;
  return cellular_device_variant;
}

bool IsFeatureEnabled(
    const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map,
    ash::multidevice_setup::mojom::Feature feature) {
  return feature_states_map.find(feature)->second ==
         ash::multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

}  // namespace

ChromeOSSystemProfileProvider::ChromeOSSystemProfileProvider()
    : cached_profile_(std::make_unique<metrics::CachedMetricsProfile>()),
      weak_ptr_factory_(this) {}

ChromeOSSystemProfileProvider::~ChromeOSSystemProfileProvider() = default;

void ChromeOSSystemProfileProvider::AsyncInit(base::OnceClosure callback) {
  base::RepeatingClosure barrier = base::BarrierClosure(4, std::move(callback));
  InitTaskGetFullHardwareClass(barrier);
  InitTaskGetArcFeatures(barrier);
  InitTaskGetTpmFirmwareVersion(barrier);
  InitTaskGetCellularDeviceVariant(barrier);
}

void ChromeOSSystemProfileProvider::OnDidCreateMetricsLog() {
  registered_user_count_at_log_initialization_ = false;
  if (user_manager::UserManager::IsInitialized()) {
    registered_user_count_at_log_initialization_ = true;
    user_count_at_log_initialization_ =
        user_manager::UserManager::Get()->GetLoggedInUsers().size();
  }
}

void ChromeOSSystemProfileProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  WriteLinkedAndroidPhoneProto(system_profile_proto);
  UpdateMultiProfileUserCount(system_profile_proto);
  WriteDemoModeDimensionMetrics(system_profile_proto);

  metrics::SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();
  hardware->set_full_hardware_class(full_hardware_class_);
  display::Display::TouchSupport has_touch =
      ui::GetInternalDisplayTouchSupport();
  if (has_touch == display::Display::TouchSupport::AVAILABLE)
    hardware->set_internal_display_supports_touch(true);
  else if (has_touch == display::Display::TouchSupport::UNAVAILABLE)
    hardware->set_internal_display_supports_touch(false);

  if (tpm_firmware_version_.has_value()) {
    hardware->set_tpm_firmware_version(*tpm_firmware_version_);
  }

  hardware->set_cellular_device_variant(cellular_device_variant_);

  if (arc_release_) {
    metrics::SystemProfileProto::OS::Arc* arc =
        system_profile_proto->mutable_os()->mutable_arc();
    arc->set_release(*arc_release_);
  }
}

void ChromeOSSystemProfileProvider::WriteLinkedAndroidPhoneProto(
    metrics::SystemProfileProto* system_profile_proto) {
  ash::multidevice_setup::MultiDeviceSetupClient* client =
      ash::multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
          cached_profile_->GetMetricsProfile());

  if (!client)
    return;

  const ash::multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
      host_status_with_device = client->GetHostStatus();
  if (host_status_with_device.first !=
      ash::multidevice_setup::mojom::HostStatus::kHostVerified) {
    return;
  }

  metrics::SystemProfileProto::LinkedAndroidPhoneData*
      linked_android_phone_data =
          system_profile_proto->mutable_linked_android_phone_data();
  const uint32_t hashed_name =
      variations::HashName(host_status_with_device.second->pii_free_name());
  linked_android_phone_data->set_phone_model_name_hash(hashed_name);

  const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
      feature_states_map = client->GetFeatureStates();
  linked_android_phone_data->set_is_smartlock_enabled(IsFeatureEnabled(
      feature_states_map, ash::multidevice_setup::mojom::Feature::kSmartLock));
  linked_android_phone_data->set_is_instant_tethering_enabled(IsFeatureEnabled(
      feature_states_map,
      ash::multidevice_setup::mojom::Feature::kInstantTethering));
  linked_android_phone_data->set_is_messages_enabled(IsFeatureEnabled(
      feature_states_map, ash::multidevice_setup::mojom::Feature::kMessages));
}

void ChromeOSSystemProfileProvider::UpdateMultiProfileUserCount(
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

void ChromeOSSystemProfileProvider::WriteDemoModeDimensionMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  if (!ash::DemoSession::IsDeviceInDemoMode()) {
    return;
  }
  metrics::SystemProfileProto::DemoModeDimensions* demo_mode_dimensions =
      system_profile_proto->mutable_demo_mode_dimensions();
  PrefService* pref = g_browser_process->local_state();
  std::string demo_country = pref->GetString(prefs::kDemoModeCountry);
  demo_mode_dimensions->set_country(demo_country);

  metrics::SystemProfileProto_DemoModeDimensions_Retailer* retailer =
      demo_mode_dimensions->mutable_retailer();
  retailer->set_retailer_id(pref->GetString(prefs::kDemoModeRetailerId));
  retailer->set_store_id(pref->GetString(prefs::kDemoModeStoreId));

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    demo_mode_dimensions->add_customization_facet(
        metrics::
            SystemProfileProto_DemoModeDimensions_CustomizationFacet_CLOUD_GAMING_DEVICE);
  }
}

void ChromeOSSystemProfileProvider::InitTaskGetFullHardwareClass(
    base::OnceClosure callback) {
  chromeos::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
          &ChromeOSSystemProfileProvider::OnMachineStatisticsLoaded,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSSystemProfileProvider::InitTaskGetArcFeatures(
    base::OnceClosure callback) {
  arc::ArcFeaturesParser::GetArcFeatures(
      base::BindOnce(&ChromeOSSystemProfileProvider::OnArcFeaturesParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSSystemProfileProvider::InitTaskGetTpmFirmwareVersion(
    base::OnceClosure callback) {
  chromeos::TpmManagerClient::Get()->GetVersionInfo(
      tpm_manager::GetVersionInfoRequest(),
      base::BindOnce(&ChromeOSSystemProfileProvider::OnTpmManagerGetVersionInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSSystemProfileProvider::InitTaskGetCellularDeviceVariant(
    base::OnceClosure callback) {
  // Run the (potentially expensive) task in the background to avoid blocking
  // the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetCellularDeviceVariantOnBackgroundThread),
      base::BindOnce(&ChromeOSSystemProfileProvider::SetCellularDeviceVariant,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOSSystemProfileProvider::OnMachineStatisticsLoaded(
    base::OnceClosure callback) {
  if (const absl::optional<base::StringPiece> full_hardware_class =
          chromeos::system::StatisticsProvider::GetInstance()
              ->GetMachineStatistic("hardware_class")) {
    full_hardware_class_ = std::string(full_hardware_class.value());
  }
  std::move(callback).Run();
}

void ChromeOSSystemProfileProvider::OnArcFeaturesParsed(
    base::OnceClosure callback,
    absl::optional<arc::ArcFeatures> features) {
  base::ScopedClosureRunner runner(std::move(callback));
  if (!features) {
    LOG(WARNING) << "ArcFeatures not available on this build";
    return;
  }
  arc_release_ = features->build_props.at("ro.build.version.release");
}

void ChromeOSSystemProfileProvider::OnTpmManagerGetVersionInfo(
    base::OnceClosure callback,
    const tpm_manager::GetVersionInfoReply& reply) {
  if (reply.status() == tpm_manager::STATUS_SUCCESS) {
    tpm_firmware_version_ = reply.firmware_version();
  } else {
    LOG(ERROR) << "Failed to get TPM version info.";
  }
  std::move(callback).Run();
}

void ChromeOSSystemProfileProvider::SetCellularDeviceVariant(
    base::OnceClosure callback,
    std::string cellular_device_variant) {
  cellular_device_variant_ = cellular_device_variant;
  std::move(callback).Run();
}
