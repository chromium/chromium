// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_provider.h"

#include <memory.h>
#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_decoder.h"
#include "chrome/browser/ash/policy/handlers/device_dlc_predownload_list_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/system_proxy_handler.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/re2/src/re2/re2.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace ash {

namespace {

// List of settings handled by the DeviceSettingsProvider.
constexpr auto kKnownSettings = base::MakeFixedFlatSet<std::string_view>({
    kAccountsPrefAllowGuest,
    kAccountsPrefAllowNewUser,
    kAccountsPrefFamilyLinkAccountsAllowed,
    kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
    kAccountsPrefDeviceLocalAccountAutoLoginDelay,
    kAccountsPrefDeviceLocalAccountAutoLoginId,
    kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
    kAccountsPrefDeviceLocalAccounts,
    kAccountsPrefEphemeralUsersEnabled,
    kAccountsPrefLoginScreenDomainAutoComplete,
    kAccountsPrefShowUserNamesOnSignIn,
    kAccountsPrefTransferSAMLCookies,
    kAccountsPrefUsers,
    kAllowBluetooth,
    kAllowedConnectionTypesForUpdate,
    kAllowRedeemChromeOsRegistrationOffers,
    kAttestationForContentProtectionEnabled,
    kCastReceiverName,
    kDeviceActivityHeartbeatCollectionRateMs,
    kDeviceActivityHeartbeatEnabled,
    kDeviceAllowedBluetoothServices,
    kDeviceAutoUpdateTimeRestrictions,
    kDeviceCrostiniArcAdbSideloadingAllowed,
    kDeviceDisabled,
    kDeviceDisabledMessage,
    kDeviceDisplayResolution,
    kDeviceDlcPredownloadList,
    kDeviceDockMacAddressSource,
    kDeviceEncryptedReportingPipelineEnabled,
    kDeviceExtendedAutoUpdateEnabled,
    kDeviceExtensionsSystemLogEnabled,
    kDeviceHindiInscriptLayoutEnabled,
    kDeviceHostnameTemplate,
    kDeviceHostnameUserConfigurable,
    kDeviceLoginScreenInputMethods,
    kDeviceLoginScreenLocales,
    kDeviceLoginScreenSystemInfoEnforced,
    kDeviceMinimumVersion,
    kDeviceMinimumVersionAueMessage,
    kDevicePeripheralDataAccessEnabled,
    kDeviceShowLowDiskSpaceNotification,
    kDeviceShowNumericKeyboardForPassword,
    kDeviceOffHours,
    kDeviceOwner,
    kDevicePrintersAccessMode,
    kDevicePrintersBlocklist,
    kDevicePrintersAllowlist,
    kDevicePrintingClientNameTemplate,
    kDevicePowerwashAllowed,
    kDeviceQuirksDownloadEnabled,
    kDeviceRebootOnUserSignout,
    kDeviceRestrictedManagedGuestSessionEnabled,
    kDeviceScheduledReboot,
    kDeviceScheduledUpdateCheck,
    kDeviceSecondFactorAuthenticationMode,
    kDeviceUnaffiliatedCrostiniAllowed,
    kDeviceWebBasedAttestationAllowedUrls,
    kDeviceWiFiAllowed,
    kDisplayRotationDefault,
    kExtensionCacheSize,
    kFeatureFlags,
    kHeartbeatEnabled,
    kHeartbeatFrequency,
    kKioskCRXManifestUpdateURLIgnored,
    kLoginAuthenticationBehavior,
    kLoginVideoCaptureAllowedUrls,
    kPluginVmAllowed,
    kPolicyMissingMitigationMode,
    kRebootOnShutdown,
    kReleaseChannel,
    kReleaseChannelDelegated,
    kReleaseLtsTag,
    kDeviceChannelDowngradeBehavior,
    kDeviceSystemAecEnabled,
    kDeviceReportRuntimeCounters,
    kDeviceReportRuntimeCountersCheckingRateMs,
    kReportDeviceActivityTimes,
    kReportDeviceAudioStatus,
    kReportDeviceAudioStatusCheckingRateMs,
    kReportDeviceBluetoothInfo,
    kReportDeviceBoardStatus,
    kReportDeviceBootMode,
    kReportDeviceCrashReportInfo,
    kReportDeviceCpuInfo,
    kReportDeviceFanInfo,
    kReportDeviceLocation,
    kReportDevicePeripherals,
    kReportDevicePowerStatus,
    kReportDeviceStorageStatus,
    kReportDeviceNetworkConfiguration,
    kReportDeviceNetworkStatus,
    kReportDeviceNetworkTelemetryCollectionRateMs,
    kReportDeviceNetworkTelemetryEventCheckingRateMs,
    kReportDeviceSessionStatus,
    kReportDeviceSecurityStatus,
    kReportDeviceSignalStrengthEventDrivenTelemetry,
    kReportDeviceTimezoneInfo,
    kReportDeviceGraphicsStatus,
    kReportDeviceMemoryInfo,
    kReportDeviceBacklightInfo,
    kReportDeviceUsers,
    kReportDeviceVersionInfo,
    kReportDeviceVpdInfo,
    kReportDeviceAppInfo,
    kReportDeviceSystemInfo,
    kReportDevicePrintJobs,
    kReportDeviceLoginLogout,
    kReportCRDSessions,
    kReportOsUpdateStatus,
    kReportRunningKioskApp,
    kReportUploadFrequency,
    kRevenEnableDeviceHWDataUsage,
    kServiceAccountIdentity,
    kSignedDataRoamingEnabled,
    kStatsReportingPref,
    kSystemLogUploadEnabled,
    kSystemProxySettings,
    kSystemTimezonePolicy,
    kSystemUse24HourClock,
    kTargetVersionPrefix,
    kTPMFirmwareUpdateSettings,
    kUnaffiliatedArcAllowed,
    kUpdateDisabled,
    kUsbDetachableAllowlist,
    kVariationsRestrictParameter,
    kVirtualMachinesAllowed,
    kDeviceReportXDREvents,
    kDeviceReportNetworkEvents,
});

constexpr char InvalidCombinationsOfAllowedUsersPoliciesHistogram[] =
    "Login.InvalidCombinationsOfAllowedUsersPolicies";

// Re-use the DecodeJsonStringAndNormalize() from device_policy_decoder.h
// here to decode the json string and validate it against |policy_name|'s
// schema. If the json string is valid, the decoded base::Value will be stored
// as |setting_name| in |pref_value_map|. The error can be ignored here since it
// is already reported during decoding in device_policy_decoder.cc.
void SetJsonDeviceSetting(const std::string& setting_name,
                          const std::string& policy_name,
                          const std::string& json_string,
                          PrefValueMap* pref_value_map) {
  auto decoding_result =
      policy::DecodeJsonStringAndNormalize(json_string, policy_name);
  if (decoding_result.has_value()) {
    pref_value_map->SetValue(setting_name,
                             std::move(decoding_result->decoded_json));
  }
}

// Re-use the DecodeDeviceDlcPredownloadListPolicy() from
// device_policy_decoder.h here to decode the list of DLCs that should be pre
// downloaded to the device.
void SetDeviceDlcPredownloadListSetting(
    const RepeatedPtrField<std::string>& raw_policy_value,
    PrefValueMap* pref_value_map) {
  std::string warning;
  base::Value::List decoded_dlc_list =
      policy::DeviceDlcPredownloadListPolicyHandler::
          DecodeDeviceDlcPredownloadListPolicy(raw_policy_value, warning);
  // The warning can be ignored here since it is already reported during
  // decoding in device_policy_decoder.cc.
  pref_value_map->SetValue(kDeviceDlcPredownloadList,
                           base::Value(std::move(decoded_dlc_list)));
}

// Puts the policy value into the settings store if only it matches the regex
// pattern.
void SetSettingWithValidatingRegex(const std::string& policy_name,
                                   const std::string& policy_value,
                                   const std::string& pattern,
                                   PrefValueMap* pref_value_map) {
  if (RE2::FullMatch(policy_value, pattern))
    pref_value_map->SetString(policy_name, policy_value);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AllowedUsersPoliciesInvalidState {
  AllowlistNotPresentAndAllowNewUsersTrue = 0,
  AllowlistNotPresentAndAllowNewUsersFalse = 1,
  AllowlistEmptyAndAllowNewUsersNotPresent = 2,
  AllowlistNonEmptyAndAllowNewUsersNotPresent = 3,
  kMaxValue = AllowlistNonEmptyAndAllowNewUsersNotPresent
};

// Returns the value of the allow_new_users (DeviceAllowNewUsers) device
// policy or an empty std::optional if the policy was not set.
std::optional<bool> GetAllowNewUsers(
    const em::ChromeDeviceSettingsProto& policy) {
  if (!policy.has_allow_new_users() ||
      !policy.allow_new_users().has_allow_new_users())
    return std::nullopt;
  return std::optional<bool>{policy.allow_new_users().allow_new_users()};
}

// Returns:
// - an empty std::optional if the user_allowlist and user_whitelist
// outer wrapper message is not present.
// - true if the user_allowlist outer wrapper message is present and the
//   user_allowlist inner list is empty, or when it's not present,
//   if the user_whitelist is non-empty.
// - false if the user_allowlist outer wrapper message is present and the
//   user_allowlist inner list has at least one element, or when it's not
//   present, and the user_whitelist has at least one element.
std::optional<bool> GetIsEmptyAllowList(
    const em::ChromeDeviceSettingsProto& policy) {
  if (policy.has_user_allowlist()) {
    base::UmaHistogramBoolean(kAllowlistCOILFallbackHistogram, false);
    return policy.user_allowlist().user_allowlist_size() == 0;
  }

  // use user_whitelist only if user_allowlist is not present
  if (policy.has_user_whitelist()) {
    base::UmaHistogramBoolean(kAllowlistCOILFallbackHistogram, true);
    return policy.user_whitelist().user_whitelist_size() == 0;
  }

  return std::nullopt;
}

// Decodes the allow_new_users (DeviceAllowNewUsers) and user_allowlist
// (DeviceUserAllowlist) policies and the guest_mode_enabled
// (DeviceGuestModeEnabled) policy.
void DecodeAllowedUsers(const em::ChromeDeviceSettingsProto& policy,
                        PrefValueMap* new_values_cache) {
  auto allow_new_users = GetAllowNewUsers(policy);
  auto is_empty_allowlist = GetIsEmptyAllowList(policy);

  if (allow_new_users.has_value() && allow_new_users.value() &&
      is_empty_allowlist.has_value() && is_empty_allowlist.value()) {
    // Allow any user to sign in
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
  } else if (allow_new_users.has_value() && !allow_new_users.value() &&
             is_empty_allowlist.has_value() && !is_empty_allowlist.value()) {
    // Restrict sign in to a list of users
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, false);
  } else if (allow_new_users.has_value() && !allow_new_users.value() &&
             is_empty_allowlist.has_value() && is_empty_allowlist.value()) {
    // Do not allow any user to sign in
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, false);
  } else if (!allow_new_users.has_value() && !is_empty_allowlist.has_value()) {
    // If policies haven't been touched, behavior is similar
    // to allow any user to sign in
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
  } else if (allow_new_users.has_value() && allow_new_users.value() &&
             is_empty_allowlist.has_value() && !is_empty_allowlist.value()) {
    // Some consumer devices out there already have this
    // combination of policies configured, the behavior is
    // similar to the first case: Allow any user to sign in
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
  } else {
    // If for some reason we encounter a combination other than
    // the 5 above, we simply default to allowing everyone to sign in
    new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);

    // Record which of the 4 invalid states we received
    if (!is_empty_allowlist.has_value() && allow_new_users.has_value()) {
      base::UmaHistogramEnumeration(
          InvalidCombinationsOfAllowedUsersPoliciesHistogram,
          allow_new_users.value()
              ? AllowedUsersPoliciesInvalidState::
                    AllowlistNotPresentAndAllowNewUsersTrue
              : AllowedUsersPoliciesInvalidState::
                    AllowlistNotPresentAndAllowNewUsersFalse);
    } else if (is_empty_allowlist.has_value() && !allow_new_users.has_value()) {
      base::UmaHistogramEnumeration(
          InvalidCombinationsOfAllowedUsersPoliciesHistogram,
          is_empty_allowlist.value()
              ? AllowedUsersPoliciesInvalidState::
                    AllowlistEmptyAndAllowNewUsersNotPresent
              : AllowedUsersPoliciesInvalidState::
                    AllowlistNonEmptyAndAllowNewUsersNotPresent);
    }
  }

  new_values_cache->SetBoolean(
      kAccountsPrefAllowGuest,
      !policy.has_guest_mode_enabled() ||
          !policy.guest_mode_enabled().has_guest_mode_enabled() ||
          policy.guest_mode_enabled().guest_mode_enabled());
}

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PrefValueMap* new_values_cache) {
  // For all our boolean settings the following is applicable:
  // true is default permissive value and false is safe prohibitive value.
  // Exceptions:
  //   kAccountsPrefEphemeralUsersEnabled has a default value of false.
  //   kAccountsPrefTransferSAMLCookies has a default value of false.
  //   kAccountsPrefFamilyLinkAccountsAllowed has a default value of false.

  // Value of DeviceFamilyLinkAccountsAllowed policy does not affect
  // |kAccountsPrefAllowNewUser| setting. Family Link accounts are only
  // allowed if user allowlist is enforced.
  DecodeAllowedUsers(policy, new_values_cache);

  bool user_allowlist_enforced =
      ((policy.has_user_whitelist() &&                         // nocheck
        policy.user_whitelist().user_whitelist_size() > 0) ||  // nocheck
       (policy.has_user_allowlist() &&
        policy.user_allowlist().user_allowlist_size() > 0));
  new_values_cache->SetBoolean(
      kAccountsPrefFamilyLinkAccountsAllowed,
      features::IsFamilyLinkOnSchoolDeviceEnabled() &&
          user_allowlist_enforced &&
          policy.has_family_link_accounts_allowed() &&
          policy.family_link_accounts_allowed()
              .has_family_link_accounts_allowed() &&
          policy.family_link_accounts_allowed().family_link_accounts_allowed());

  new_values_cache->SetBoolean(
      kRebootOnShutdown,
      policy.has_reboot_on_shutdown() &&
          policy.reboot_on_shutdown().has_reboot_on_shutdown() &&
          policy.reboot_on_shutdown().reboot_on_shutdown());

  new_values_cache->SetBoolean(
      kAccountsPrefShowUserNamesOnSignIn,
      !policy.has_show_user_names() ||
          !policy.show_user_names().has_show_user_names() ||
          policy.show_user_names().show_user_names());

  new_values_cache->SetBoolean(
      kAccountsPrefEphemeralUsersEnabled,
      policy.has_ephemeral_users_enabled() &&
          policy.ephemeral_users_enabled().has_ephemeral_users_enabled() &&
          policy.ephemeral_users_enabled().ephemeral_users_enabled());

  {
    base::Value::List list;
    const em::UserAllowlistProto& allowlist_proto = policy.user_allowlist();
    if (policy.user_allowlist().user_allowlist_size() > 0) {
      const RepeatedPtrField<std::string>& allowlist =
          allowlist_proto.user_allowlist();
      for (const std::string& value : allowlist) {
        list.Append(value);
      }
    } else {
      const em::UserWhitelistProto& whitelist_proto =   // nocheck
          policy.user_whitelist();                      // nocheck
      const RepeatedPtrField<std::string>& whitelist =  // nocheck
          whitelist_proto.user_whitelist();             // nocheck
      for (const std::string& value : whitelist) {      // nocheck
        list.Append(value);
      }
    }
    new_values_cache->SetValue(kAccountsPrefUsers,
                               base::Value(std::move(list)));
  }

  base::Value::List account_list;
  const em::DeviceLocalAccountsProto device_local_accounts_proto =
      policy.device_local_accounts();
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      device_local_accounts_proto.account();
  for (const em::DeviceLocalAccountInfoProto& entry : accounts) {
    base::Value::Dict entry_dict;
    if (entry.has_type()) {
      if (entry.has_account_id()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyId,
                       entry.account_id());
      }
      entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyType, entry.type());
      if (entry.kiosk_app().has_app_id()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                       entry.kiosk_app().app_id());
      }
      if (entry.kiosk_app().has_update_url()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                       entry.kiosk_app().update_url());
      }
      if (entry.web_kiosk_app().has_url()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                       entry.web_kiosk_app().url());
      }
      if (entry.web_kiosk_app().has_title()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
                       entry.web_kiosk_app().title());
      }
      if (entry.web_kiosk_app().has_icon_url()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
                       entry.web_kiosk_app().icon_url());
      }
      if (entry.has_ephemeral_mode()) {
        entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                       static_cast<int>(entry.ephemeral_mode()));
      } else {
        entry_dict.Set(
            kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
            static_cast<int>(
                em::DeviceLocalAccountInfoProto::EPHEMERAL_MODE_UNSET));
      }
      if (entry.has_isolated_kiosk_app()) {
        if (entry.isolated_kiosk_app().has_web_bundle_id()) {
          entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
                         entry.isolated_kiosk_app().web_bundle_id());
        }
        if (entry.isolated_kiosk_app().has_update_manifest_url()) {
          entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
                         entry.isolated_kiosk_app().update_manifest_url());
        }
      }
    } else if (entry.has_deprecated_public_session_id()) {
      // Deprecated public session specification.
      entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyId,
                     entry.deprecated_public_session_id());
      entry_dict.Set(
          kAccountsPrefDeviceLocalAccountsKeyType,
          em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    }
    account_list.Append(std::move(entry_dict));
  }
  new_values_cache->SetValue(kAccountsPrefDeviceLocalAccounts,
                             base::Value(std::move(account_list)));

  if (policy.has_device_local_accounts()) {
    if (policy.device_local_accounts().has_auto_login_id()) {
      new_values_cache->SetString(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          policy.device_local_accounts().auto_login_id());
    }
    if (policy.device_local_accounts().has_auto_login_delay()) {
      new_values_cache->SetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          policy.device_local_accounts().auto_login_delay());
    }
  }

  new_values_cache->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      policy.device_local_accounts().enable_auto_login_bailout());
  new_values_cache->SetBoolean(
      kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
      policy.device_local_accounts().prompt_for_network_when_offline());

  if (policy.has_feature_flags()) {
    base::Value::List feature_flags_list;
    for (const std::string& entry : policy.feature_flags().feature_flags()) {
      feature_flags_list.Append(entry);
    }
    if (!feature_flags_list.empty()) {
      new_values_cache->SetValue(kFeatureFlags,
                                 base::Value(std::move(feature_flags_list)));
    }
  }

  if (policy.has_saml_settings()) {
    new_values_cache->SetBoolean(
        kAccountsPrefTransferSAMLCookies,
        policy.saml_settings().transfer_saml_cookies());
  }

  // The behavior when policy is not set and when it is set to an empty string
  // is the same. Thus lets add policy only if it is set and its value is not
  // an empty string.
  if (policy.has_login_screen_domain_auto_complete() &&
      policy.login_screen_domain_auto_complete()
          .has_login_screen_domain_auto_complete() &&
      !policy.login_screen_domain_auto_complete()
           .login_screen_domain_auto_complete()
           .empty()) {
    SetSettingWithValidatingRegex(kAccountsPrefLoginScreenDomainAutoComplete,
                                  policy.login_screen_domain_auto_complete()
                                      .login_screen_domain_auto_complete(),
                                  policy::hostNameRegex, new_values_cache);
  }

  if (policy.has_login_authentication_behavior() &&
      policy.login_authentication_behavior()
          .has_login_authentication_behavior()) {
    new_values_cache->SetInteger(
        kLoginAuthenticationBehavior,
        policy.login_authentication_behavior().login_authentication_behavior());
  }

  if (policy.has_login_video_capture_allowed_urls()) {
    base::Value::List list;
    const em::LoginVideoCaptureAllowedUrlsProto&
        login_video_capture_allowed_urls_proto =
            policy.login_video_capture_allowed_urls();
    for (const auto& value : login_video_capture_allowed_urls_proto.urls()) {
      list.Append(value);
    }
    new_values_cache->SetValue(kLoginVideoCaptureAllowedUrls,
                               base::Value(std::move(list)));
  }

  if (policy.has_login_screen_locales()) {
    base::Value::List locales;
    const em::LoginScreenLocalesProto& login_screen_locales(
        policy.login_screen_locales());
    for (const auto& locale : login_screen_locales.login_screen_locales())
      locales.Append(locale);
    new_values_cache->SetValue(kDeviceLoginScreenLocales,
                               base::Value(std::move(locales)));
  }

  if (policy.has_login_screen_input_methods()) {
    base::Value::List input_methods;
    const em::LoginScreenInputMethodsProto& login_screen_input_methods(
        policy.login_screen_input_methods());
    for (const auto& input_method :
         login_screen_input_methods.login_screen_input_methods())
      input_methods.Append(input_method);
    new_values_cache->SetValue(kDeviceLoginScreenInputMethods,
                               base::Value(std::move(input_methods)));
  }

  if (policy.has_device_login_screen_system_info_enforced() &&
      policy.device_login_screen_system_info_enforced().has_value()) {
    new_values_cache->SetBoolean(
        kDeviceLoginScreenSystemInfoEnforced,
        policy.device_login_screen_system_info_enforced().value());
  }

  if (policy.has_device_show_numeric_keyboard_for_password() &&
      policy.device_show_numeric_keyboard_for_password().has_value()) {
    new_values_cache->SetBoolean(
        kDeviceShowNumericKeyboardForPassword,
        policy.device_show_numeric_keyboard_for_password().value());
  }

  if (policy.has_device_web_based_attestation_allowed_urls()) {
    const em::StringListPolicyProto& container(
        policy.device_web_based_attestation_allowed_urls());

    base::Value::List urls;
    for (const std::string& entry : container.value().entries()) {
      urls.Append(entry);
    }

    new_values_cache->SetValue(kDeviceWebBasedAttestationAllowedUrls,
                               base::Value(std::move(urls)));
  }

  if (policy.has_kiosk_crx_manifest_update_url_ignored()) {
    const em::BooleanPolicyProto& container(
        policy.kiosk_crx_manifest_update_url_ignored());

    if (container.has_value()) {
      new_values_cache->SetValue(kKioskCRXManifestUpdateURLIgnored,
                                 base::Value(container.value()));
    }
  }
}

void DecodeNetworkPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PrefValueMap* new_values_cache) {
  // kSignedDataRoamingEnabled has a default value of false.
  new_values_cache->SetBoolean(
      kSignedDataRoamingEnabled,
      policy.has_data_roaming_enabled() &&
          policy.data_roaming_enabled().has_data_roaming_enabled() &&
          policy.data_roaming_enabled().data_roaming_enabled());
  if (policy.has_system_proxy_settings()) {
    const em::SystemProxySettingsProto& settings_proto(
        policy.system_proxy_settings());
    if (settings_proto.has_system_proxy_settings()) {
      SetJsonDeviceSetting(
          kSystemProxySettings, policy::key::kSystemProxySettings,
          settings_proto.system_proxy_settings(), new_values_cache);
    }
  }
}

void DecodeAutoUpdatePolicies(const em::ChromeDeviceSettingsProto& policy,
                              PrefValueMap* new_values_cache) {
  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& au_settings_proto =
        policy.auto_update_settings();
    if (au_settings_proto.has_update_disabled()) {
      new_values_cache->SetBoolean(kUpdateDisabled,
                                   au_settings_proto.update_disabled());
    }

    if (au_settings_proto.has_target_version_prefix()) {
      new_values_cache->SetString(kTargetVersionPrefix,
                                  au_settings_proto.target_version_prefix());
    }

    const RepeatedField<int>& allowed_connection_types =
        au_settings_proto.allowed_connection_types();
    base::Value::List list;
    for (int value : allowed_connection_types) {
      list.Append(value);
    }
    if (!list.empty()) {
      new_values_cache->SetValue(kAllowedConnectionTypesForUpdate,
                                 base::Value(std::move(list)));
    }

    if (au_settings_proto.has_disallowed_time_intervals()) {
      SetJsonDeviceSetting(kDeviceAutoUpdateTimeRestrictions,
                           policy::key::kDeviceAutoUpdateTimeRestrictions,
                           au_settings_proto.disallowed_time_intervals(),
                           new_values_cache);
    }

    if (au_settings_proto.has_channel_downgrade_behavior()) {
      new_values_cache->SetValue(
          kDeviceChannelDowngradeBehavior,
          base::Value(au_settings_proto.channel_downgrade_behavior()));
    }
  }

  if (policy.has_device_scheduled_update_check()) {
    const em::DeviceScheduledUpdateCheckProto& scheduled_update_check_policy =
        policy.device_scheduled_update_check();
    if (scheduled_update_check_policy
            .has_device_scheduled_update_check_settings()) {
      SetJsonDeviceSetting(kDeviceScheduledUpdateCheck,
                           policy::key::kDeviceScheduledUpdateCheck,
                           scheduled_update_check_policy
                               .device_scheduled_update_check_settings(),
                           new_values_cache);
    }
  }

  if (policy.has_deviceextendedautoupdateenabled()) {
    const em::BooleanPolicyProto& container(
        policy.deviceextendedautoupdateenabled());
    if (container.has_value()) {
      new_values_cache->SetValue(kDeviceExtendedAutoUpdateEnabled,
                                 base::Value(container.value()));
    }
  }
}

void DecodeReportingPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PrefValueMap* new_values_cache) {
  if (policy.has_device_reporting()) {
    const em::DeviceReportingProto& reporting_policy =
        policy.device_reporting();
    if (reporting_policy.has_report_version_info()) {
      new_values_cache->SetBoolean(kReportDeviceVersionInfo,
                                   reporting_policy.report_version_info());
    }
    if (reporting_policy.has_report_activity_times()) {
      new_values_cache->SetBoolean(kReportDeviceActivityTimes,
                                   reporting_policy.report_activity_times());
    }
    if (reporting_policy.has_report_audio_status()) {
      new_values_cache->SetBoolean(kReportDeviceAudioStatus,
                                   reporting_policy.report_audio_status());
    }
    if (reporting_policy.has_report_runtime_counters()) {
      new_values_cache->SetBoolean(kDeviceReportRuntimeCounters,
                                   reporting_policy.report_runtime_counters());
    }
    if (reporting_policy.has_report_boot_mode()) {
      new_values_cache->SetBoolean(kReportDeviceBootMode,
                                   reporting_policy.report_boot_mode());
    }
    if (reporting_policy.has_report_crash_report_info()) {
      new_values_cache->SetBoolean(kReportDeviceCrashReportInfo,
                                   reporting_policy.report_crash_report_info());
    }
    if (reporting_policy.has_report_network_configuration()) {
      new_values_cache->SetBoolean(
          kReportDeviceNetworkConfiguration,
          reporting_policy.report_network_configuration());
    }
    if (reporting_policy.has_report_network_status()) {
      new_values_cache->SetBoolean(kReportDeviceNetworkStatus,
                                   reporting_policy.report_network_status());
    }
    if (reporting_policy.has_report_users()) {
      new_values_cache->SetBoolean(kReportDeviceUsers,
                                   reporting_policy.report_users());
    }
    if (reporting_policy.has_report_session_status()) {
      new_values_cache->SetBoolean(kReportDeviceSessionStatus,
                                   reporting_policy.report_session_status());
    }
    if (reporting_policy.has_report_security_status()) {
      new_values_cache->SetBoolean(kReportDeviceSecurityStatus,
                                   reporting_policy.report_security_status());
    }
    if (reporting_policy.has_report_graphics_status()) {
      new_values_cache->SetBoolean(kReportDeviceGraphicsStatus,
                                   reporting_policy.report_graphics_status());
    }
    if (reporting_policy.has_report_os_update_status()) {
      new_values_cache->SetBoolean(kReportOsUpdateStatus,
                                   reporting_policy.report_os_update_status());
    }
    if (reporting_policy.has_report_running_kiosk_app()) {
      new_values_cache->SetBoolean(kReportRunningKioskApp,
                                   reporting_policy.report_running_kiosk_app());
    }
    if (reporting_policy.has_report_peripherals()) {
      new_values_cache->SetBoolean(kReportDevicePeripherals,
                                   reporting_policy.report_peripherals());
    }
    if (reporting_policy.has_report_power_status()) {
      new_values_cache->SetBoolean(kReportDevicePowerStatus,
                                   reporting_policy.report_power_status());
    }
    if (reporting_policy.has_report_storage_status()) {
      new_values_cache->SetBoolean(kReportDeviceStorageStatus,
                                   reporting_policy.report_storage_status());
    }
    if (reporting_policy.has_report_board_status()) {
      new_values_cache->SetBoolean(kReportDeviceBoardStatus,
                                   reporting_policy.report_board_status());
    }
    if (reporting_policy.has_device_status_frequency()) {
      new_values_cache->SetInteger(kReportUploadFrequency,
                                   reporting_policy.device_status_frequency());
    }
    if (reporting_policy.has_report_cpu_info()) {
      new_values_cache->SetBoolean(kReportDeviceCpuInfo,
                                   reporting_policy.report_cpu_info());
    }
    if (reporting_policy.has_report_timezone_info()) {
      new_values_cache->SetBoolean(kReportDeviceTimezoneInfo,
                                   reporting_policy.report_timezone_info());
    }
    if (reporting_policy.has_report_memory_info()) {
      new_values_cache->SetBoolean(kReportDeviceMemoryInfo,
                                   reporting_policy.report_memory_info());
    }
    if (reporting_policy.has_report_backlight_info()) {
      new_values_cache->SetBoolean(kReportDeviceBacklightInfo,
                                   reporting_policy.report_backlight_info());
    }
    if (reporting_policy.has_report_app_info()) {
      new_values_cache->SetBoolean(kReportDeviceAppInfo,
                                   reporting_policy.report_app_info());
    }
    if (reporting_policy.has_report_bluetooth_info()) {
      new_values_cache->SetBoolean(kReportDeviceBluetoothInfo,
                                   reporting_policy.report_bluetooth_info());
    }
    if (reporting_policy.has_report_fan_info()) {
      new_values_cache->SetBoolean(kReportDeviceFanInfo,
                                   reporting_policy.report_fan_info());
    }
    if (reporting_policy.has_report_vpd_info()) {
      new_values_cache->SetBoolean(kReportDeviceVpdInfo,
                                   reporting_policy.report_vpd_info());
    }
    if (reporting_policy.has_report_system_info()) {
      new_values_cache->SetBoolean(kReportDeviceSystemInfo,
                                   reporting_policy.report_system_info());
    }
    if (reporting_policy.has_report_print_jobs()) {
      new_values_cache->SetBoolean(kReportDevicePrintJobs,
                                   reporting_policy.report_print_jobs());
    }
    if (reporting_policy.has_report_login_logout()) {
      new_values_cache->SetBoolean(kReportDeviceLoginLogout,
                                   reporting_policy.report_login_logout());
    }
    if (reporting_policy.has_report_crd_sessions()) {
      new_values_cache->SetBoolean(kReportCRDSessions,
                                   reporting_policy.report_crd_sessions());
    }
    if (reporting_policy.has_report_network_telemetry_collection_rate_ms()) {
      new_values_cache->SetInteger(
          kReportDeviceNetworkTelemetryCollectionRateMs,
          reporting_policy.report_network_telemetry_collection_rate_ms());
    }
    if (reporting_policy
            .has_report_network_telemetry_event_checking_rate_ms()) {
      new_values_cache->SetInteger(
          kReportDeviceNetworkTelemetryEventCheckingRateMs,
          reporting_policy.report_network_telemetry_event_checking_rate_ms());
    }
    if (reporting_policy.has_report_device_audio_status_checking_rate_ms()) {
      new_values_cache->SetInteger(
          kReportDeviceAudioStatusCheckingRateMs,
          reporting_policy.report_device_audio_status_checking_rate_ms());
    }
    if (reporting_policy
            .has_device_report_runtime_counters_checking_rate_ms()) {
      new_values_cache->SetInteger(
          kDeviceReportRuntimeCountersCheckingRateMs,
          reporting_policy.device_report_runtime_counters_checking_rate_ms());
    }
    if (reporting_policy.has_report_signal_strength_event_driven_telemetry()) {
      base::Value::List signal_strength_telemetry_list;
      for (const std::string& telemetry_entry :
           reporting_policy.report_signal_strength_event_driven_telemetry()
               .entries()) {
        signal_strength_telemetry_list.Append(telemetry_entry);
      }
      new_values_cache->SetValue(
          kReportDeviceSignalStrengthEventDrivenTelemetry,
          base::Value(std::move(signal_strength_telemetry_list)));
    }
    if (reporting_policy.has_device_activity_heartbeat_enabled()) {
      new_values_cache->SetBoolean(
          kDeviceActivityHeartbeatEnabled,
          reporting_policy.device_activity_heartbeat_enabled());
    }
    if (reporting_policy.has_device_activity_heartbeat_collection_rate_ms()) {
      new_values_cache->SetInteger(
          kDeviceActivityHeartbeatCollectionRateMs,
          reporting_policy.device_activity_heartbeat_collection_rate_ms());
    }
    if (reporting_policy.has_report_network_events()) {
      new_values_cache->SetBoolean(kDeviceReportNetworkEvents,
                                   reporting_policy.report_network_events());
    }
  }
}

void DecodeHeartbeatPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PrefValueMap* new_values_cache) {
  if (!policy.has_device_heartbeat_settings())
    return;

  const em::DeviceHeartbeatSettingsProto& heartbeat_policy =
      policy.device_heartbeat_settings();
  if (heartbeat_policy.has_heartbeat_enabled()) {
    new_values_cache->SetBoolean(kHeartbeatEnabled,
                                 heartbeat_policy.heartbeat_enabled());
  }
  if (heartbeat_policy.has_heartbeat_frequency()) {
    new_values_cache->SetInteger(kHeartbeatFrequency,
                                 heartbeat_policy.heartbeat_frequency());
  }
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PrefValueMap* new_values_cache) {
  if (policy.has_metrics_enabled() &&
      policy.metrics_enabled().has_metrics_enabled()) {
    new_values_cache->SetBoolean(kStatsReportingPref,
                                 policy.metrics_enabled().metrics_enabled());
  } else {
    // If the policy is missing, default to reporting enabled on enterprise-
    // enrolled devices, c.f. crbug/456186.
    new_values_cache->SetBoolean(
        kStatsReportingPref, InstallAttributes::Get()->IsEnterpriseManaged());
  }

  if (policy.has_device_system_aec_enabled() &&
      policy.device_system_aec_enabled().has_device_system_aec_enabled()) {
    new_values_cache->SetBoolean(
        kDeviceSystemAecEnabled,
        policy.device_system_aec_enabled().device_system_aec_enabled());
  }

  if (!policy.has_release_channel() ||
      !policy.release_channel().has_release_channel()) {
    // Default to an invalid channel (will be ignored).
    new_values_cache->SetString(kReleaseChannel, "");
  } else {
    new_values_cache->SetString(kReleaseChannel,
                                policy.release_channel().release_channel());
  }

  new_values_cache->SetBoolean(
      kReleaseChannelDelegated,
      policy.has_release_channel() &&
          policy.release_channel().has_release_channel_delegated() &&
          policy.release_channel().release_channel_delegated());

  if (policy.has_release_channel()) {
    if (policy.release_channel().has_release_lts_tag()) {
      new_values_cache->SetString(kReleaseLtsTag,
                                  policy.release_channel().release_lts_tag());
    }
  }

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      new_values_cache->SetString(kSystemTimezonePolicy,
                                  policy.system_timezone().timezone());
    }
  }

  if (policy.has_use_24hour_clock()) {
    if (policy.use_24hour_clock().has_use_24hour_clock()) {
      new_values_cache->SetBoolean(
          kSystemUse24HourClock, policy.use_24hour_clock().use_24hour_clock());
    }
  }

  if (policy.has_allow_redeem_offers() &&
      policy.allow_redeem_offers().has_allow_redeem_offers()) {
    new_values_cache->SetBoolean(
        kAllowRedeemChromeOsRegistrationOffers,
        policy.allow_redeem_offers().allow_redeem_offers());
  } else {
    new_values_cache->SetBoolean(kAllowRedeemChromeOsRegistrationOffers, true);
  }

  if (policy.has_variations_parameter()) {
    new_values_cache->SetString(kVariationsRestrictParameter,
                                policy.variations_parameter().parameter());
  }

  if (policy.has_attestation_settings() &&
      policy.attestation_settings().has_content_protection_enabled()) {
    new_values_cache->SetBoolean(
        kAttestationForContentProtectionEnabled,
        policy.attestation_settings().content_protection_enabled());
  } else {
    new_values_cache->SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

  bool is_device_pci_peripheral_data_access_enabled = false;
  if (policy.has_device_pci_peripheral_data_access_enabled_v2()) {
    const em::DevicePciPeripheralDataAccessEnabledProtoV2& container(
        policy.device_pci_peripheral_data_access_enabled_v2());
    if (container.has_enabled()) {
      is_device_pci_peripheral_data_access_enabled = container.enabled();
    }
  }
  new_values_cache->SetBoolean(kDevicePeripheralDataAccessEnabled,
                               is_device_pci_peripheral_data_access_enabled);

  if (policy.has_extension_cache_size() &&
      policy.extension_cache_size().has_extension_cache_size()) {
    new_values_cache->SetInteger(
        kExtensionCacheSize,
        policy.extension_cache_size().extension_cache_size());
  }

  if (policy.has_display_rotation_default() &&
      policy.display_rotation_default().has_display_rotation_default()) {
    new_values_cache->SetInteger(
        kDisplayRotationDefault,
        policy.display_rotation_default().display_rotation_default());
  }

  if (policy.has_device_display_resolution() &&
      policy.device_display_resolution().has_device_display_resolution()) {
    SetJsonDeviceSetting(
        kDeviceDisplayResolution, policy::key::kDeviceDisplayResolution,
        policy.device_display_resolution().device_display_resolution(),
        new_values_cache);
  } else {
    // Set empty value if policy is missing, to make sure that webui
    // will receive setting update.
    new_values_cache->SetValue(kDeviceDisplayResolution,
                               base::Value(base::Value::Type::DICT));
  }

  if (policy.has_allow_bluetooth() &&
      policy.allow_bluetooth().has_allow_bluetooth()) {
    new_values_cache->SetBoolean(kAllowBluetooth,
                                 policy.allow_bluetooth().allow_bluetooth());
  } else {
    new_values_cache->SetBoolean(kAllowBluetooth, true);
  }

  if (policy.has_device_wifi_allowed() &&
      policy.device_wifi_allowed().has_device_wifi_allowed()) {
    new_values_cache->SetBoolean(
        kDeviceWiFiAllowed, policy.device_wifi_allowed().device_wifi_allowed());
  } else {
    new_values_cache->SetBoolean(kDeviceWiFiAllowed, true);
  }

  if (policy.has_quirks_download_enabled() &&
      policy.quirks_download_enabled().has_quirks_download_enabled()) {
    new_values_cache->SetBoolean(
        kDeviceQuirksDownloadEnabled,
        policy.quirks_download_enabled().quirks_download_enabled());
  }

  if (policy.has_device_off_hours()) {
    auto off_hours_policy = policy::off_hours::ConvertOffHoursProtoToValue(
        policy.device_off_hours());
    if (off_hours_policy) {
      new_values_cache->SetValue(kDeviceOffHours,
                                 base::Value(std::move(*off_hours_policy)));
    }
  }

  if (policy.has_tpm_firmware_update_settings()) {
    new_values_cache->SetValue(kTPMFirmwareUpdateSettings,
                               tpm_firmware_update::DecodeSettingsProto(
                                   policy.tpm_firmware_update_settings()));
  }

  if (policy.has_device_minimum_version()) {
    const em::StringPolicyProto& container(policy.device_minimum_version());
    if (container.has_value()) {
      SetJsonDeviceSetting(kDeviceMinimumVersion,
                           policy::key::kDeviceMinimumVersion,
                           container.value(), new_values_cache);
    }
  }

  if (policy.has_device_minimum_version_aue_message()) {
    const em::StringPolicyProto& container(
        policy.device_minimum_version_aue_message());
    if (container.has_value()) {
      new_values_cache->SetValue(kDeviceMinimumVersionAueMessage,
                                 base::Value(container.value()));
    }
  }

  if (policy.has_cast_receiver_name()) {
    const em::CastReceiverNameProto& container(policy.cast_receiver_name());
    if (container.has_name()) {
      new_values_cache->SetValue(kCastReceiverName,
                                 base::Value(container.name()));
    }
  }

  if (policy.has_unaffiliated_arc_allowed()) {
    const em::UnaffiliatedArcAllowedProto& container(
        policy.unaffiliated_arc_allowed());
    if (container.has_unaffiliated_arc_allowed()) {
      new_values_cache->SetValue(
          kUnaffiliatedArcAllowed,
          base::Value(container.unaffiliated_arc_allowed()));
    }
  }

  if (policy.has_network_hostname()) {
    const em::NetworkHostnameProto& container(policy.network_hostname());
    if (container.has_device_hostname_template() &&
        !container.device_hostname_template().empty()) {
      new_values_cache->SetString(kDeviceHostnameTemplate,
                                  container.device_hostname_template());
    }
  }

  if (policy.has_hostname_user_configurable()) {
    const em::HostnameUserConfigurableProto& container(
        policy.hostname_user_configurable());
    if (container.has_device_hostname_user_configurable()) {
      new_values_cache->SetBoolean(
          kDeviceHostnameUserConfigurable,
          container.device_hostname_user_configurable());
    }
  }

  if (policy.virtual_machines_allowed().has_virtual_machines_allowed()) {
    new_values_cache->SetBoolean(
        kVirtualMachinesAllowed,
        policy.virtual_machines_allowed().virtual_machines_allowed());
  }

  if (policy.has_device_unaffiliated_crostini_allowed()) {
    const em::DeviceUnaffiliatedCrostiniAllowedProto& container(
        policy.device_unaffiliated_crostini_allowed());
    if (container.has_device_unaffiliated_crostini_allowed()) {
      new_values_cache->SetValue(
          kDeviceUnaffiliatedCrostiniAllowed,
          base::Value(container.device_unaffiliated_crostini_allowed()));
    }
  }

  if (policy.has_plugin_vm_allowed()) {
    const em::PluginVmAllowedProto& container(policy.plugin_vm_allowed());
    if (container.has_plugin_vm_allowed()) {
      new_values_cache->SetValue(kPluginVmAllowed,
                                 base::Value(container.plugin_vm_allowed()));
    }
  }

  // Default value of the policy in case it's missing.
  int access_mode = em::DevicePrintersAccessModeProto::ACCESS_MODE_ALL;
  // Use DevicePrintersAccessMode policy if present, otherwise Native version.
  if (policy.has_device_printers_access_mode() &&
      policy.device_printers_access_mode().has_access_mode()) {
    access_mode = policy.device_printers_access_mode().access_mode();
    if (!em::DevicePrintersAccessModeProto::AccessMode_IsValid(access_mode)) {
      LOG(ERROR) << "Unrecognized device native printers access mode";
      // If the policy is outside the range of allowed values, default to
      // AllowAll.
      access_mode = em::DevicePrintersAccessModeProto::ACCESS_MODE_ALL;
    }
  } else if (policy.has_native_device_printers_access_mode() &&
             policy.native_device_printers_access_mode().has_access_mode()) {
    access_mode = policy.native_device_printers_access_mode().access_mode();
    if (!em::DevicePrintersAccessModeProto::AccessMode_IsValid(access_mode)) {
      LOG(ERROR) << "Unrecognized device native printers access mode";
      // If the policy is outside the range of allowed values, default to
      // AllowAll.
      access_mode = em::DevicePrintersAccessModeProto::ACCESS_MODE_ALL;
    }
  }
  new_values_cache->SetInteger(kDevicePrintersAccessMode, access_mode);

  // Use Blocklist policy if present, otherwise Blacklist version.  // nocheck
  if (policy.has_device_printers_blocklist()) {
    base::Value::List list;
    const em::DevicePrintersBlocklistProto& proto(
        policy.device_printers_blocklist());
    for (const auto& id : proto.blocklist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersBlocklist,
                               base::Value(std::move(list)));
  }

  // Use Allowlist policy if present, otherwise Whitelist version.  // nocheck
  if (policy.has_device_printers_allowlist()) {
    base::Value::List list;
    const em::DevicePrintersAllowlistProto& proto(
        policy.device_printers_allowlist());
    for (const auto& id : proto.allowlist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersAllowlist,
                               base::Value(std::move(list)));
  }

  if (policy.has_device_printing_client_name_template()) {
    const em::StringPolicyProto& container(
        policy.device_printing_client_name_template());
    if (container.has_value() && !container.value().empty()) {
      new_values_cache->SetString(kDevicePrintingClientNameTemplate,
                                  container.value());
    }
  }

  if (policy.has_device_reboot_on_user_signout()) {
    const em::DeviceRebootOnUserSignoutProto& container(
        policy.device_reboot_on_user_signout());
    if (container.has_reboot_on_signout_mode()) {
      new_values_cache->SetValue(
          kDeviceRebootOnUserSignout,
          base::Value(container.reboot_on_signout_mode()));
    }
  }

  int dock_mac_address_source =
      em::DeviceDockMacAddressSourceProto::DOCK_NIC_MAC_ADDRESS;
  if (policy.has_device_dock_mac_address_source() &&
      policy.device_dock_mac_address_source().has_source()) {
    dock_mac_address_source = policy.device_dock_mac_address_source().source();
  }
  new_values_cache->SetInteger(kDeviceDockMacAddressSource,
                               dock_mac_address_source);

  if (policy.has_device_second_factor_authentication() &&
      policy.device_second_factor_authentication().has_mode()) {
    new_values_cache->SetInteger(
        kDeviceSecondFactorAuthenticationMode,
        policy.device_second_factor_authentication().mode());
  }

  // Default value of the policy in case it's missing.
  bool is_powerwash_allowed = true;
  if (policy.has_device_powerwash_allowed()) {
    const em::DevicePowerwashAllowedProto& container(
        policy.device_powerwash_allowed());
    if (container.has_device_powerwash_allowed()) {
      is_powerwash_allowed = container.device_powerwash_allowed();
    }
  }
  new_values_cache->SetBoolean(kDevicePowerwashAllowed, is_powerwash_allowed);

  if (policy.has_device_crostini_arc_adb_sideloading_allowed()) {
    const em::DeviceCrostiniArcAdbSideloadingAllowedProto& container(
        policy.device_crostini_arc_adb_sideloading_allowed());
    if (container.has_mode()) {
      new_values_cache->SetValue(kDeviceCrostiniArcAdbSideloadingAllowed,
                                 base::Value(container.mode()));
    }
  }

  // Default value of the policy in case it's missing.
  bool show_low_disk_space_notification = true;
  // Disable the notification by default for enrolled devices.
  if (InstallAttributes::Get()->IsEnterpriseManaged())
    show_low_disk_space_notification = false;
  if (policy.has_device_show_low_disk_space_notification()) {
    const em::DeviceShowLowDiskSpaceNotificationProto& container(
        policy.device_show_low_disk_space_notification());
    if (container.has_device_show_low_disk_space_notification()) {
      show_low_disk_space_notification =
          container.device_show_low_disk_space_notification();
    }
  }
  new_values_cache->SetBoolean(kDeviceShowLowDiskSpaceNotification,
                               show_low_disk_space_notification);

  if (policy.has_usb_detachable_allowlist() &&
      policy.usb_detachable_allowlist().id_size() > 0) {
    const em::UsbDetachableAllowlistProto& container =
        policy.usb_detachable_allowlist();
    base::Value::List allowlist;
    for (const auto& entry : container.id()) {
      base::Value::Dict ids;
      if (entry.has_vendor_id() && entry.has_product_id()) {
        ids.Set(kUsbDetachableAllowlistKeyVid, entry.vendor_id());
        ids.Set(kUsbDetachableAllowlistKeyPid, entry.product_id());
      }
      allowlist.Append(std::move(ids));
    }
    new_values_cache->SetValue(kUsbDetachableAllowlist,
                               base::Value(std::move(allowlist)));
  }

  if (policy.has_device_allowed_bluetooth_services()) {
    base::Value::List list;
    const em::DeviceAllowedBluetoothServicesProto& container(
        policy.device_allowed_bluetooth_services());
    for (const auto& service_uuid : container.allowlist())
      list.Append(service_uuid);
    new_values_cache->SetValue(kDeviceAllowedBluetoothServices,
                               base::Value(std::move(list)));
  }

  if (policy.has_device_scheduled_reboot()) {
    const em::DeviceScheduledRebootProto& scheduled_reboot_policy =
        policy.device_scheduled_reboot();
    if (scheduled_reboot_policy.has_device_scheduled_reboot_settings()) {
      SetJsonDeviceSetting(
          kDeviceScheduledReboot, policy::key::kDeviceScheduledReboot,
          scheduled_reboot_policy.device_scheduled_reboot_settings(),
          new_values_cache);
    }
  }

  if (policy.has_device_restricted_managed_guest_session_enabled()) {
    const em::DeviceRestrictedManagedGuestSessionEnabledProto& container(
        policy.device_restricted_managed_guest_session_enabled());
    if (container.has_enabled()) {
      new_values_cache->SetValue(kDeviceRestrictedManagedGuestSessionEnabled,
                                 base::Value(container.enabled()));
    }
  }

  bool reven_enable_device_hw_data_usage =
      policy.has_hardware_data_usage_enabled() &&
      policy.hardware_data_usage_enabled().has_hardware_data_usage_enabled() &&
      policy.hardware_data_usage_enabled().hardware_data_usage_enabled();
  new_values_cache->SetBoolean(kRevenEnableDeviceHWDataUsage,
                               reven_enable_device_hw_data_usage);

  if (policy.has_device_encrypted_reporting_pipeline_enabled()) {
    const em::EncryptedReportingPipelineConfigurationProto& container(
        policy.device_encrypted_reporting_pipeline_enabled());
    if (container.has_enabled()) {
      new_values_cache->SetValue(kDeviceEncryptedReportingPipelineEnabled,
                                 base::Value(container.enabled()));
    }
  }

  if (policy.has_device_report_xdr_events()) {
    const em::DeviceReportXDREventsProto& container(
        policy.device_report_xdr_events());
    if (container.has_enabled()) {
      new_values_cache->SetValue(kDeviceReportXDREvents,
                                 base::Value(container.enabled()));
    }
  }

  if (policy.has_device_hindi_inscript_layout_enabled()) {
    const em::DeviceHindiInscriptLayoutEnabledProto& container(
        policy.device_hindi_inscript_layout_enabled());
    if (container.has_enabled()) {
      new_values_cache->SetValue(kDeviceHindiInscriptLayoutEnabled,
                                 base::Value(container.enabled()));
    }
  }

  if (policy.has_device_dlc_predownload_list()) {
    SetDeviceDlcPredownloadListSetting(
        policy.device_dlc_predownload_list().value().entries(),
        new_values_cache);
  }

  if (policy.has_deviceextensionssystemlogenabled()) {
    const em::BooleanPolicyProto& container(
        policy.deviceextensionssystemlogenabled());
    if (container.has_value()) {
      new_values_cache->SetValue(kDeviceExtensionsSystemLogEnabled,
                                 base::Value(container.value()));
    }
  }
}

void DecodeLogUploadPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PrefValueMap* new_values_cache) {
  if (!policy.has_device_log_upload_settings())
    return;

  const em::DeviceLogUploadSettingsProto& log_upload_policy =
      policy.device_log_upload_settings();
  if (log_upload_policy.has_system_log_upload_enabled()) {
    new_values_cache->SetBoolean(kSystemLogUploadEnabled,
                                 log_upload_policy.system_log_upload_enabled());
  }
}

void DecodeDeviceState(const em::PolicyData& policy_data,
                       PrefValueMap* new_values_cache) {
  if (!policy_data.has_device_state())
    return;

  const em::DeviceState& device_state = policy_data.device_state();

  if (device_state.device_mode() == em::DeviceState::DEVICE_MODE_DISABLED)
    new_values_cache->SetBoolean(kDeviceDisabled, true);
  if (device_state.has_disabled_state() &&
      device_state.disabled_state().has_message()) {
    new_values_cache->SetString(kDeviceDisabledMessage,
                                device_state.disabled_state().message());
  }
}

}  // namespace

DeviceSettingsProvider::DeviceSettingsProvider(
    const NotifyObserversCallback& notify_cb,
    DeviceSettingsService* device_settings_service,
    PrefService* local_state)
    : CrosSettingsProvider(notify_cb),
      device_settings_service_(device_settings_service),
      local_state_(local_state),
      trusted_status_(TEMPORARILY_UNTRUSTED),
      ownership_status_(device_settings_service_->GetOwnershipStatus()) {
  device_settings_service_->AddObserver(this);
  if (!UpdateFromService()) {
    // Make sure we have at least the cache data immediately.
    RetrieveCachedData();
  }
}

DeviceSettingsProvider::~DeviceSettingsProvider() {
  if (device_settings_service_->GetOwnerSettingsService())
    device_settings_service_->GetOwnerSettingsService()->RemoveObserver(this);
  device_settings_service_->RemoveObserver(this);
}

// static
bool DeviceSettingsProvider::IsDeviceSetting(std::string_view name) {
  return kKnownSettings.contains(name);
}

// static
void DeviceSettingsProvider::DecodePolicies(
    const em::ChromeDeviceSettingsProto& policy,
    PrefValueMap* new_values_cache) {
  DecodeLoginPolicies(policy, new_values_cache);
  DecodeNetworkPolicies(policy, new_values_cache);
  DecodeAutoUpdatePolicies(policy, new_values_cache);
  DecodeReportingPolicies(policy, new_values_cache);
  DecodeHeartbeatPolicies(policy, new_values_cache);
  DecodeGenericPolicies(policy, new_values_cache);
  DecodeLogUploadPolicies(policy, new_values_cache);
}

void DeviceSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  // Make sure that either the current user is the device owner or the
  // device doesn't have an owner yet.
  if (!(device_settings_service_->HasPrivateOwnerKey() ||
        ownership_status_ ==
            DeviceSettingsService::OwnershipStatus::kOwnershipNone)) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    NotifyObservers(path);
    return;
  }

  if (!IsDeviceSetting(path)) {
    NOTREACHED_IN_MIGRATION() << "Try to set unhandled cros setting " << path;
    return;
  }

  if (device_settings_service_->HasPrivateOwnerKey()) {
    // Directly set setting through OwnerSettingsService.
    ownership::OwnerSettingsService* service =
        device_settings_service_->GetOwnerSettingsService();
    if (!service->Set(path, in_value)) {
      NotifyObservers(path);
      return;
    }
  } else {
    // Temporary store new setting in
    // |device_settings_|. |device_settings_| will be stored on a disk
    // as soon as an ownership of device the will be taken.
    OwnerSettingsServiceAsh::UpdateDeviceSettings(path, in_value,
                                                  device_settings_);
    em::PolicyData data;
    data.set_username(device_settings_service_->GetUsername());
    CHECK(device_settings_.SerializeToString(data.mutable_policy_value()));

    // Set the cache to the updated value.
    UpdateValuesCache(data, device_settings_, TEMPORARILY_UNTRUSTED);

    if (!device_settings_cache::Store(data, local_state_)) {
      LOG(ERROR) << "Couldn't store to the temp storage.";
      NotifyObservers(path);
      return;
    }
  }
}

void DeviceSettingsProvider::OwnershipStatusChanged() {
  DeviceSettingsService::OwnershipStatus new_ownership_status =
      device_settings_service_->GetOwnershipStatus();

  if (device_settings_service_->GetOwnerSettingsService())
    device_settings_service_->GetOwnerSettingsService()->AddObserver(this);

  // If the device just became owned, write the settings accumulated in the
  // cache to device settings proper. It is important that writing only happens
  // in this case, as during normal operation, the contents of the cache should
  // never overwrite actual device settings.
  if (new_ownership_status ==
          DeviceSettingsService::OwnershipStatus::kOwnershipTaken &&
      ownership_status_ ==
          DeviceSettingsService::OwnershipStatus::kOwnershipNone) {
    if (device_settings_service_->HasPrivateOwnerKey()) {
      // There shouldn't be any pending writes, since the cache writes are all
      // immediate.
      DCHECK(!store_callback_factory_.HasWeakPtrs());

      trusted_status_ = TEMPORARILY_UNTRUSTED;
      // Apply the locally-accumulated device settings on top of the initial
      // settings from the service and write back the result.
      if (device_settings_service_->device_settings()) {
        em::ChromeDeviceSettingsProto new_settings(
            *device_settings_service_->device_settings());
        new_settings.MergeFrom(device_settings_);
        device_settings_.Swap(&new_settings);
      }

      std::unique_ptr<em::PolicyData> policy(new em::PolicyData());
      policy->set_username(device_settings_service_->GetUsername());
      CHECK(device_settings_.SerializeToString(policy->mutable_policy_value()));
      if (!device_settings_service_->GetOwnerSettingsService()
               ->CommitTentativeDeviceSettings(std::move(policy))) {
        LOG(ERROR) << "Can't store policy";
      }

      // TODO(crbug.com/41143265): Some of the above code can be
      // simplified or removed, once the DoSet function is removed - then there
      // will be no pending writes. This is because the only values that need to
      // be written as a pending write is kStatsReportingPref and
      // kEnableDeviceHWDataUsage, and those are now handled by the Controllers
      // - see below. Once DoSet is removed and there are no pending writes that
      // are being maintained by DeviceSettingsProvider, this code for updating
      // the signed settings for the new owner should probably be moved outside
      // of DeviceSettingsProvider.

      StatsReportingController::Get()->OnOwnershipTaken(
          device_settings_service_->GetOwnerSettingsService());
      HWDataUsageController::Get()->OnOwnershipTaken(
          device_settings_service_->GetOwnerSettingsService());
    } else if (InstallAttributes::Get()->IsEnterpriseManaged()) {
      StatsReportingController::Get()->ClearPendingValue();
      HWDataUsageController::Get()->ClearPendingValue();
    }
  }

  ownership_status_ = new_ownership_status;
}

void DeviceSettingsProvider::DeviceSettingsUpdated() {
  if (!store_callback_factory_.HasWeakPtrs())
    UpdateAndProceedStoring();
}

void DeviceSettingsProvider::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

void DeviceSettingsProvider::OnTentativeChangesInPolicy(
    const em::PolicyData& policy_data) {
  em::ChromeDeviceSettingsProto device_settings;
  CHECK(device_settings.ParseFromString(policy_data.policy_value()));
  UpdateValuesCache(policy_data, device_settings, TEMPORARILY_UNTRUSTED);
}

void DeviceSettingsProvider::RetrieveCachedData() {
  em::PolicyData policy_data;
  if (!device_settings_cache::Retrieve(&policy_data, local_state_) ||
      !device_settings_.ParseFromString(policy_data.policy_value())) {
    VLOG(1) << "Can't retrieve temp store, possibly not created yet.";
  }

  UpdateValuesCache(policy_data, device_settings_, trusted_status_);
}

void DeviceSettingsProvider::UpdateValuesCache(
    const em::PolicyData& policy_data,
    const em::ChromeDeviceSettingsProto& settings,
    TrustedStatus trusted_status) {
  PrefValueMap new_values_cache;

  // Determine whether device is managed. See PolicyData::management_mode docs
  // for details.
  bool managed = false;
  if (policy_data.has_management_mode()) {
    managed =
        (policy_data.management_mode() == em::PolicyData::ENTERPRISE_MANAGED);
  } else {
    managed = policy_data.has_request_token();
  }

  // If the device is not managed, we set the device owner value.
  if (policy_data.has_username() && !managed)
    new_values_cache.SetString(kDeviceOwner, policy_data.username());

  if (policy_data.has_service_account_identity()) {
    new_values_cache.SetString(kServiceAccountIdentity,
                               policy_data.service_account_identity());
  }

  DecodePolicies(settings, &new_values_cache);
  DecodeDeviceState(policy_data, &new_values_cache);

  // Collect all notifications but send them only after we have swapped the
  // cache so that if somebody actually reads the cache will be already valid.
  std::vector<std::string> notifications;
  // Go through the new values and verify in the old ones.
  auto iter = new_values_cache.begin();
  for (; iter != new_values_cache.end(); ++iter) {
    const base::Value* old_value;
    if (!values_cache_.GetValue(iter->first, &old_value) ||
        *old_value != iter->second) {
      notifications.push_back(iter->first);
    }
  }
  // Now check for values that have been removed from the policy blob.
  for (iter = values_cache_.begin(); iter != values_cache_.end(); ++iter) {
    const base::Value* value;
    if (!new_values_cache.GetValue(iter->first, &value))
      notifications.push_back(iter->first);
  }
  // Swap and notify.
  values_cache_.Swap(&new_values_cache);
  trusted_status_ = trusted_status;
  for (size_t i = 0; i < notifications.size(); ++i)
    NotifyObservers(notifications[i]);
}

bool DeviceSettingsProvider::MitigateMissingPolicy() {
  // First check if the device has been owned already and if not exit
  // immediately.
  if (InstallAttributes::Get()->GetMode() != policy::DEVICE_MODE_CONSUMER)
    return false;

  // If we are here the policy file were corrupted or missing. This can happen
  // because we are migrating Pre R11 device to the new secure policies or there
  // was an attempt to circumvent policy system. In this case we should populate
  // the policy cache with "safe-mode" defaults which should allow the owner to
  // log in but lock the device for anyone else until the policy blob has been
  // recreated by the session manager.
  LOG(ERROR) << "Corruption of the policy data has been detected."
             << "Switching to \"safe-mode\" policies until the owner logs in "
             << "to regenerate the policy data.";
  base::UmaHistogramBoolean("Enterprise.DeviceSettings.MissingPolicyMitigated",
                            true);

  device_settings_.Clear();
  device_settings_.mutable_allow_new_users()->set_allow_new_users(true);
  device_settings_.mutable_user_allowlist()->clear_user_allowlist();
  device_settings_.mutable_guest_mode_enabled()->set_guest_mode_enabled(true);
  em::PolicyData empty_policy_data;
  UpdateValuesCache(empty_policy_data, device_settings_, TRUSTED);
  values_cache_.SetBoolean(kPolicyMissingMitigationMode, true);

  return true;
}

const base::Value* DeviceSettingsProvider::Get(std::string_view path) const {
  if (IsDeviceSetting(path)) {
    const base::Value* value;
    if (values_cache_.GetValue(path, &value))
      return value;
  } else {
    NOTREACHED_IN_MIGRATION() << "Trying to get non cros setting.";
  }

  return nullptr;
}

DeviceSettingsProvider::TrustedStatus
DeviceSettingsProvider::PrepareTrustedValues(base::OnceClosure* callback) {
  TrustedStatus status = RequestTrustedEntity();
  if (status == TEMPORARILY_UNTRUSTED && *callback)
    callbacks_.push_back(std::move(*callback));
  return status;
}

bool DeviceSettingsProvider::HandlesSetting(std::string_view path) const {
  return IsDeviceSetting(path);
}

DeviceSettingsProvider::TrustedStatus
DeviceSettingsProvider::RequestTrustedEntity() {
  if (ownership_status_ ==
      DeviceSettingsService::OwnershipStatus::kOwnershipNone) {
    return TRUSTED;
  }
  return trusted_status_;
}

void DeviceSettingsProvider::UpdateAndProceedStoring() {
  // Re-sync the cache from the service.
  UpdateFromService();
}

bool DeviceSettingsProvider::UpdateFromService() {
  bool settings_loaded = false;
  base::UmaHistogramEnumeration("Enterprise.DeviceSettings.UpdatedStatus",
                                device_settings_service_->status());
  switch (device_settings_service_->status()) {
    case DeviceSettingsService::STORE_SUCCESS: {
      const em::PolicyData* policy_data =
          device_settings_service_->policy_data();
      const em::ChromeDeviceSettingsProto* device_settings =
          device_settings_service_->device_settings();
      if (policy_data && device_settings) {
        if (!device_settings_cache::Store(*policy_data, local_state_)) {
          LOG(ERROR) << "Couldn't update the local state cache.";
        }
        UpdateValuesCache(*policy_data, *device_settings, TRUSTED);
        device_settings_ = *device_settings;

        settings_loaded = true;
      } else {
        // Initial policy load is still pending.
        trusted_status_ = TEMPORARILY_UNTRUSTED;
      }
      break;
    }
    case DeviceSettingsService::STORE_NO_POLICY:
      if (MitigateMissingPolicy())
        break;
      [[fallthrough]];
    case DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      if (user_manager::UserManager::Get()->GetOwnerEmail().has_value()) {
        // On the consumer owned device Chrome is responsible for generating a
        // new key and/or policy if they are missing (which happens after the
        // user session starts).
        trusted_status_ = TEMPORARILY_UNTRUSTED;
      } else {
        VLOG(1) << "No policies present yet, will use the temp storage.";
        trusted_status_ = PERMANENTLY_UNTRUSTED;
      }
      break;
    case DeviceSettingsService::STORE_VALIDATION_ERROR:
    case DeviceSettingsService::STORE_INVALID_POLICY:
    case DeviceSettingsService::STORE_OPERATION_FAILED: {
      DeviceSettingsService::Status status = device_settings_service_->status();
      LOG(ERROR) << "Failed to retrieve cros policies. Reason: " << status
                 << " (" << DeviceSettingsService::StatusToString(status)
                 << ")";
      trusted_status_ = PERMANENTLY_UNTRUSTED;
      break;
    }
  }

  // Notify the observers we are done.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  return settings_loaded;
}

}  // namespace ash
