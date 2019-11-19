// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service.h"

#include <map>
#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/services/device_sync/public/cpp/gcm_constants.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {

namespace {

const char kInstanceIdScope[] = "GCM";

const cryptauthv2::FeatureMetadata& GenerateFeatureMetadata() {
  static const base::NoDestructor<cryptauthv2::FeatureMetadata>
      feature_metadata([] {
        cryptauthv2::BetterTogetherFeatureMetadata inner_metadata;

        // Smart Lock, MultiDevice Setup and Messages are supported on all
        // Chromebooks.
        inner_metadata.add_supported_features(
            cryptauthv2::
                BetterTogetherFeatureMetadata_FeatureName_EASY_UNLOCK_CLIENT);
        inner_metadata.add_supported_features(
            cryptauthv2::
                BetterTogetherFeatureMetadata_FeatureName_BETTER_TOGETHER_CLIENT);
        inner_metadata.add_supported_features(
            cryptauthv2::
                BetterTogetherFeatureMetadata_FeatureName_SMS_CONNECT_CLIENT);

        // Instant Tethering is only supported if the associated flag enabled.
        if (base::FeatureList::IsEnabled(features::kInstantTethering)) {
          inner_metadata.add_supported_features(
              cryptauthv2::
                  BetterTogetherFeatureMetadata_FeatureName_MAGIC_TETHER_CLIENT);
        }

        // Note: |inner_metadata|'s enabled_features field is deprecated and
        // left unset here (the server ignores this value when processing the
        // received proto).

        cryptauthv2::FeatureMetadata feature_metadata;
        feature_metadata.set_feature_type(
            cryptauthv2::FeatureMetadata_Feature_BETTER_TOGETHER);
        feature_metadata.set_metadata(inner_metadata.SerializeAsString());

        return feature_metadata;
      }());

  return *feature_metadata;
}

cryptauthv2::ApplicationSpecificMetadata GenerateApplicationSpecificMetadata(
    const std::string& gcm_registration_id,
    int64_t software_version_code) {
  cryptauthv2::ApplicationSpecificMetadata metadata;
  metadata.set_gcm_registration_id(gcm_registration_id);
  // Chrome OS system notifications are always enabled.
  metadata.set_notification_enabled(true);
  metadata.set_device_software_version(base::GetLinuxDistro());
  metadata.set_device_software_version_code(software_version_code);
  metadata.set_device_software_package(device_sync::kCryptAuthGcmAppId);
  return metadata;
}

void LogInstanceIdTokenFetchRetries(int count) {
  base::UmaHistogramExactLinear(
      "CryptAuth.ClientAppMetadataInstanceIdTokenFetch.Retries", count, 2);
}

}  // namespace

// static
int64_t ClientAppMetadataProviderService::ConvertVersionCodeToInt64(
    const std::string& version_code_str) {
  static const size_t kNumDigitsInLastSection = 3;
  std::string version_code_copy = version_code_str;

  size_t last_period_index = version_code_copy.rfind('.');
  if (last_period_index != std::string::npos) {
    // If there are fewer than |kNumDigitsInLastSection| digits in the last
    // section, add extra '0' characters.
    size_t num_digits_to_add = kNumDigitsInLastSection + last_period_index -
                               (version_code_copy.size() - 1u);
    version_code_copy.insert(last_period_index + 1u /* pos */,
                             std::string(num_digits_to_add, '0') /* str */);
  }

  int64_t code = 0;
  for (auto it = version_code_copy.cbegin(); it != version_code_copy.cend();
       ++it) {
    if (*it < '0' || *it > '9')
      continue;
    code = code * 10 + (*it - '0');
  }

  return code;
}

ClientAppMetadataProviderService::ClientAppMetadataProviderService(
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    instance_id::InstanceIDProfileService* instance_id_profile_service)
    : pref_service_(pref_service),
      network_state_handler_(network_state_handler),
      instance_id_profile_service_(instance_id_profile_service) {}

ClientAppMetadataProviderService::~ClientAppMetadataProviderService() {
  // If there are any pending callbacks, invoke them before this object is
  // deleted.
  InvokePendingCallbacks();
}

void ClientAppMetadataProviderService::GetClientAppMetadata(
    const std::string& gcm_registration_id,
    GetMetadataCallback callback) {
  pending_callbacks_.push_back(std::move(callback));

  // If the metadata has already been computed, provide it to the callback
  // immediately.
  if (client_app_metadata_) {
    // If |gcm_registration_id| is different from a previously-supplied ID,
    // replace the ID with this new one.
    DCHECK_EQ(1, client_app_metadata_->application_specific_metadata_size());
    client_app_metadata_->mutable_application_specific_metadata(0 /* index */)
        ->set_gcm_registration_id(gcm_registration_id);

    InvokePendingCallbacks();
    return;
  }

  // If |instance_id_profile_service_| is null, Shutdown() has been called and
  // there should be no further attempt to calculate the ClientAppMetadata,
  // since this could result in touching deleted memory.
  if (!instance_id_profile_service_) {
    InvokePendingCallbacks();
    return;
  }

  bool was_already_in_progress = pending_gcm_registration_id_.has_value();
  pending_gcm_registration_id_ = gcm_registration_id;

  // If metadata is currently being computed, update
  // |pending_gcm_registration_id_| and wait for the ongoing attempt to complete
  // before continuing.
  if (was_already_in_progress)
    return;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&ClientAppMetadataProviderService::OnBluetoothAdapterFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ClientAppMetadataProviderService::Shutdown() {
  // Null out |instance_id_profile_service_| to signify that it should no longer
  // be used.
  instance_id_profile_service_ = nullptr;

  // If the ClientAppMetadata is currently being computed and this class is
  // waiting for an asynchronous operation to return, stop the computation now
  // to ensure that deleted memory is not touched.
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_gcm_registration_id_.reset();

  InvokePendingCallbacks();
}

void ClientAppMetadataProviderService::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  base::SysInfo::GetHardwareInfo(
      base::Bind(&ClientAppMetadataProviderService::OnHardwareInfoFetched,
                 weak_ptr_factory_.GetWeakPtr(), bluetooth_adapter));
}

void ClientAppMetadataProviderService::OnHardwareInfoFetched(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    base::SysInfo::HardwareInfo hardware_info) {
  GetInstanceId()->GetID(base::Bind(
      &ClientAppMetadataProviderService::OnInstanceIdFetched,
      weak_ptr_factory_.GetWeakPtr(), bluetooth_adapter, hardware_info));
}

void ClientAppMetadataProviderService::OnInstanceIdFetched(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    const base::SysInfo::HardwareInfo& hardware_info,
    const std::string& instance_id) {
  DCHECK(!instance_id.empty());
  GetInstanceId()->GetToken(
      device_sync::
          kCryptAuthV2EnrollmentAuthorizedEntity /* authorized_entity */,
      kInstanceIdScope /* scope */,
      std::map<std::string, std::string>() /* options */, {} /* flags */,
      base::Bind(&ClientAppMetadataProviderService::OnInstanceIdTokenFetched,
                 weak_ptr_factory_.GetWeakPtr(), bluetooth_adapter,
                 hardware_info, instance_id));
}

void ClientAppMetadataProviderService::OnInstanceIdTokenFetched(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    const base::SysInfo::HardwareInfo& hardware_info,
    const std::string& instance_id,
    const std::string& token,
    instance_id::InstanceID::Result result) {
  // If the |token| doesn't begin with the |instance_id|, we have to re-create
  // the entire InstanceID and remove the old one from storage.
  if (token.find(':') != std::string::npos &&
      !base::StartsWith(token, instance_id,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    GetInstanceId()->DeleteID(base::BindOnce(
        &ClientAppMetadataProviderService::OnInstanceIdDeleted,
        weak_ptr_factory_.GetWeakPtr(), bluetooth_adapter, hardware_info));
    return;
  }
  LogInstanceIdTokenFetchRetries(instance_id_recreated_ ? 1 : 0);

  std::string gcm_registration_id = *pending_gcm_registration_id_;
  pending_gcm_registration_id_.reset();
  instance_id_recreated_ = false;

  UMA_HISTOGRAM_ENUMERATION(
      "CryptAuth.ClientAppMetadataInstanceIdTokenFetch.Result", result,
      instance_id::InstanceID::Result::LAST_RESULT + 1);

  // If fetching the token failed, invoke the pending callbacks with a null
  // ClientAppMetadata.
  if (result != instance_id::InstanceID::Result::SUCCESS) {
    PA_LOG(WARNING) << "ClientAppMetadataProviderService::"
                    << "OnInstanceIdTokenFetched(): Failed to fetch InstanceID "
                    << "token; result: " << result << ".";
    InvokePendingCallbacks();
    return;
  }

  DCHECK(!token.empty());

  cryptauthv2::ClientAppMetadata metadata;

  metadata.add_application_specific_metadata()->CopyFrom(
      GenerateApplicationSpecificMetadata(gcm_registration_id,
                                          SoftwareVersionCodeAsInt64()));
  metadata.set_instance_id(instance_id);
  metadata.set_instance_id_token(token);
  metadata.set_long_device_id(
      cryptauth::CryptAuthDeviceIdProviderImpl::GetInstance()->GetDeviceId());

  metadata.set_locale(ChromeContentBrowserClient().GetApplicationLocale());
  metadata.set_device_os_version(base::GetLinuxDistro());
  metadata.set_device_os_version_code(SoftwareVersionCodeAsInt64());
  metadata.set_device_os_release(version_info::GetVersionNumber());
  metadata.set_device_os_codename(version_info::GetProductName());

  // device_display_diagonal_mils is unused because it only applies to
  // phones/tablets.
  metadata.set_device_display_diagonal_mils(0);
  metadata.set_device_model(hardware_info.model);
  metadata.set_device_manufacturer(hardware_info.manufacturer);
  metadata.set_device_type(cryptauthv2::ClientAppMetadata_DeviceType_CHROME);

  metadata.set_using_secure_screenlock(
      pref_service_->GetBoolean(ash::prefs::kEnableAutoScreenLock));
  // Auto-unlock here refers to concepts such as "trusted places" and "trusted
  // peripherals." Chromebooks do support Smart Lock (i.e., unlocking via the
  // user's phone), but these fields are unrelated.
  metadata.set_auto_unlock_screenlock_supported(false);
  metadata.set_auto_unlock_screenlock_enabled(false);

  metadata.set_bluetooth_radio_supported(bluetooth_adapter->IsPresent());
  metadata.set_bluetooth_radio_enabled(bluetooth_adapter->IsPowered());
  // Within Chrome, there is no way to determine if Bluetooth Classic vs. BLE is
  // supported. Since BLE was released in ~2011, it is a safe assumption that
  // devices still receiving Chrome OS updates do support BLE, so rely on
  // BluetoothAdapter::IsPresent() for this field as well.
  metadata.set_ble_radio_supported(bluetooth_adapter->IsPresent());

  metadata.set_mobile_data_supported(
      network_state_handler_->IsTechnologyAvailable(
          NetworkTypePattern::Cellular()));
  // The tethering_supported field does not refer to use of Instant Tethering;
  // rather, this indicates that Chrome OS devices cannot create their own WiFi
  // hotspots.
  metadata.set_tethering_supported(false);
  metadata.add_feature_metadata()->CopyFrom(GenerateFeatureMetadata());

  // Note: |metadata|'s bluetooth_address field is not set since enrollment
  // occurs before any opt-in which would alert the users that their Bluetooth
  // addresses are being uploaded.

  // "Pixel experience" refers only to Android devices, so set this field to
  // false even if this Chromebook is a Pixelbook, Pixel Slate, etc.
  metadata.set_pixel_experience(false);
  // |metadata| is being constructed in the browser process (i.e., outside of
  // the ARC++ container).
  metadata.set_arc_plus_plus(false);
  metadata.set_hardware_user_presence_supported(false);
  metadata.set_user_verification_supported(true);
  metadata.set_trusted_execution_environment_supported(false);
  metadata.set_dedicated_secure_element_supported(false);

  client_app_metadata_ = metadata;
  InvokePendingCallbacks();
}

void ClientAppMetadataProviderService::OnInstanceIdDeleted(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    const base::SysInfo::HardwareInfo& hardware_info,
    instance_id::InstanceID::Result result) {
  instance_id_profile_service_->driver()->RemoveInstanceID(
      device_sync::kCryptAuthGcmAppId);

  if (instance_id_recreated_) {
    LogInstanceIdTokenFetchRetries(2);
    PA_LOG(WARNING) << "ClientAppMetadataProviderService::"
                    << "OnInstanceIdDeleted(): Instance Id deleted twice in a "
                    << "row, aborting; result: " << result << ".";
    pending_gcm_registration_id_.reset();
    instance_id_recreated_ = false;
    InvokePendingCallbacks();
    return;
  }

  instance_id_recreated_ = true;
  OnHardwareInfoFetched(bluetooth_adapter, hardware_info);
}

instance_id::InstanceID* ClientAppMetadataProviderService::GetInstanceId() {
  DCHECK(instance_id_profile_service_);
  DCHECK(instance_id_profile_service_->driver());
  return instance_id_profile_service_->driver()->GetInstanceID(
      device_sync::kCryptAuthGcmAppId);
}

int64_t ClientAppMetadataProviderService::SoftwareVersionCodeAsInt64() {
  static const int64_t version_code =
      ConvertVersionCodeToInt64(version_info::GetVersionNumber());
  return version_code;
}

void ClientAppMetadataProviderService::InvokePendingCallbacks() {
  auto it = pending_callbacks_.begin();
  while (it != pending_callbacks_.end()) {
    std::move(*it).Run(client_app_metadata_);
    it = pending_callbacks_.erase(it);
  }
}

}  // namespace chromeos
