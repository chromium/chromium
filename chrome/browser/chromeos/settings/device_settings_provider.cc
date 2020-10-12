// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <memory.h>
#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/chromeos/policy/system_proxy_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/re2/src/re2/re2.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace chromeos {

namespace {

// List of settings handled by the DeviceSettingsProvider.
const char* const kKnownSettings[] = {
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
    kAccountsPrefSupervisedUsersEnabled,
    kAccountsPrefTransferSAMLCookies,
    kAccountsPrefUsers,
    kAllowBluetooth,
    kAllowedConnectionTypesForUpdate,
    kAllowRedeemChromeOsRegistrationOffers,
    kAttestationForContentProtectionEnabled,
    kCastReceiverName,
    kDeviceAttestationEnabled,
    kDeviceAutoUpdateTimeRestrictions,
    kDeviceCrostiniArcAdbSideloadingAllowed,
    kDeviceDisabled,
    kDeviceDisabledMessage,
    kDeviceDisplayResolution,
    kDeviceDockMacAddressSource,
    kDeviceHostnameTemplate,
    kDeviceLoginScreenExtensions,
    kDeviceLoginScreenInputMethods,
    kDeviceLoginScreenLocales,
    kDeviceLoginScreenSystemInfoEnforced,
    kDeviceMinimumVersion,
    kDeviceMinimumVersionAueMessage,
    kDeviceShowLowDiskSpaceNotification,
    kDeviceShowNumericKeyboardForPassword,
    kDeviceOffHours,
    kDeviceOwner,
    kDevicePrintersAccessMode,
    kDevicePrintersBlocklist,
    kDevicePrintersAllowlist,
    kDevicePowerwashAllowed,
    kDeviceQuirksDownloadEnabled,
    kDeviceRebootOnUserSignout,
    kDeviceScheduledUpdateCheck,
    kDeviceSecondFactorAuthenticationMode,
    kDeviceUnaffiliatedCrostiniAllowed,
    kDeviceWebBasedAttestationAllowedUrls,
    kDeviceWiFiAllowed,
    kDeviceWilcoDtcAllowed,
    kDisplayRotationDefault,
    kExtensionCacheSize,
    kHeartbeatEnabled,
    kHeartbeatFrequency,
    kLoginAuthenticationBehavior,
    kLoginVideoCaptureAllowedUrls,
    kPluginVmAllowed,
    kPluginVmLicenseKey,
    kPolicyMissingMitigationMode,
    kRebootOnShutdown,
    kReleaseChannel,
    kReleaseChannelDelegated,
    kReleaseLtsTag,
    kDeviceChannelDowngradeBehavior,
    kReportDeviceActivityTimes,
    kReportDeviceBluetoothInfo,
    kReportDeviceBoardStatus,
    kReportDeviceBootMode,
    kReportDeviceCrashReportInfo,
    kReportDeviceCpuInfo,
    kReportDeviceFanInfo,
    kReportDeviceHardwareStatus,
    kReportDeviceLocation,
    kReportDevicePowerStatus,
    kReportDeviceStorageStatus,
    kReportDeviceNetworkInterfaces,
    kReportDeviceSessionStatus,
    kReportDeviceTimezoneInfo,
    kReportDeviceGraphicsStatus,
    kReportDeviceMemoryInfo,
    kReportDeviceBacklightInfo,
    kReportDeviceUsers,
    kReportDeviceVersionInfo,
    kReportDeviceVpdInfo,
    kReportDeviceAppInfo,
    kReportDeviceSystemInfo,
    kReportOsUpdateStatus,
    kReportRunningKioskApp,
    kReportUploadFrequency,
    kSamlLoginAuthenticationType,
    kServiceAccountIdentity,
    kSignedDataRoamingEnabled,
    kStartUpFlags,
    kStatsReportingPref,
    kSystemLogUploadEnabled,
    kSystemProxySettings,
    kSystemTimezonePolicy,
    kSystemUse24HourClock,
    kTargetVersionPrefix,
    kTPMFirmwareUpdateSettings,
    kUnaffiliatedArcAllowed,
    kUpdateDisabled,
    kVariationsRestrictParameter,
    kVirtualMachinesAllowed,
};

// Re-use the DecodeJsonStringAndNormalize from device_policy_decoder_chromeos.h
// here to decode the json string and validate it against |policy_name|'s
// schema. If the json string is valid, the decoded base::Value will be stored
// as |setting_name| in |pref_value_map|. The error can be ignored here since it
// is already reported during decoding in device_policy_decoder_chromeos.cc.
void SetJsonDeviceSetting(const std::string& setting_name,
                          const std::string& policy_name,
                          const std::string& json_string,
                          PrefValueMap* pref_value_map) {
  std::string error;
  base::Optional<base::Value> decoded_json =
      policy::DecodeJsonStringAndNormalize(json_string, policy_name, &error);
  if (decoded_json.has_value()) {
    pref_value_map->SetValue(setting_name, std::move(decoded_json.value()));
  }
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

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PrefValueMap* new_values_cache) {
  // For all our boolean settings the following is applicable:
  // true is default permissive value and false is safe prohibitive value.
  // Exceptions:
  //   kAccountsPrefEphemeralUsersEnabled has a default value of false.
  //   kAccountsPrefSupervisedUsersEnabled has a default value of false
  //     for enterprise devices and true for consumer devices.
  //   kAccountsPrefTransferSAMLCookies has a default value of false.
  //   kAccountsPrefFamilyLinkAccountsAllowed has a default value of false.
  if (policy.has_allow_new_users() &&
      policy.allow_new_users().has_allow_new_users()) {
    if (policy.allow_new_users().allow_new_users()) {
      // New users allowed, user whitelist ignored.
      new_values_cache->SetBoolean(kAccountsPrefAllowNewUser, true);
    } else {
      // New users not allowed, enforce user allowlist if present.
      new_values_cache->SetBoolean(
          kAccountsPrefAllowNewUser,
          !policy.has_user_whitelist() && !policy.has_user_allowlist());
    }
  } else {
    // No configured allow-new-users value, enforce whitelist if non-empty.
    new_values_cache->SetBoolean(
        kAccountsPrefAllowNewUser,
        policy.user_whitelist().user_whitelist_size() == 0 &&
            policy.user_allowlist().user_allowlist_size() == 0);
  }

  // Value of DeviceFamilyLinkAccountsAllowed policy does not affect
  // |kAccountsPrefAllowNewUser| setting. Family Link accounts are only
  // allowed if user allowlist is enforced.
  bool user_allowlist_enforced =
      ((policy.has_user_whitelist() &&
        policy.user_whitelist().user_whitelist_size() > 0) ||
       (policy.has_user_allowlist() &&
        policy.user_allowlist().user_allowlist_size() > 0));
  new_values_cache->SetBoolean(
      kAccountsPrefFamilyLinkAccountsAllowed,
      chromeos::features::IsFamilyLinkOnSchoolDeviceEnabled() &&
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
      kAccountsPrefAllowGuest,
      !policy.has_guest_mode_enabled() ||
          !policy.guest_mode_enabled().has_guest_mode_enabled() ||
          policy.guest_mode_enabled().guest_mode_enabled());

  bool supervised_users_enabled = false;
  if (!InstallAttributes::Get()->IsEnterpriseManaged()) {
    supervised_users_enabled = true;
  }
  new_values_cache->SetBoolean(kAccountsPrefSupervisedUsersEnabled,
                               supervised_users_enabled);

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

  std::vector<base::Value> list;
  const em::UserAllowlistProto& allowlist_proto = policy.user_allowlist();
  if (policy.user_allowlist().user_allowlist_size() > 0) {
    const RepeatedPtrField<std::string>& allowlist =
        allowlist_proto.user_allowlist();
    for (const std::string& value : allowlist) {
      list.push_back(base::Value(value));
    }
  } else {
    const em::UserWhitelistProto& whitelist_proto = policy.user_whitelist();
    const RepeatedPtrField<std::string>& whitelist =
        whitelist_proto.user_whitelist();
    for (const std::string& value : whitelist) {
      list.push_back(base::Value(value));
    }
  }

  new_values_cache->SetValue(kAccountsPrefUsers, base::Value(std::move(list)));

  std::vector<base::Value> account_list;
  const em::DeviceLocalAccountsProto device_local_accounts_proto =
      policy.device_local_accounts();
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      device_local_accounts_proto.account();
  RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
  for (const em::DeviceLocalAccountInfoProto& entry : accounts) {
    base::Value entry_dict(base::Value::Type::DICTIONARY);
    if (entry.has_type()) {
      if (entry.has_account_id()) {
        entry_dict.SetKey(kAccountsPrefDeviceLocalAccountsKeyId,
                          base::Value(entry.account_id()));
      }
      entry_dict.SetKey(kAccountsPrefDeviceLocalAccountsKeyType,
                        base::Value(entry.type()));
      if (entry.kiosk_app().has_app_id()) {
        entry_dict.SetKey(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                          base::Value(entry.kiosk_app().app_id()));
      }
      if (entry.kiosk_app().has_update_url()) {
        entry_dict.SetKey(kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                          base::Value(entry.kiosk_app().update_url()));
      }
      if (entry.android_kiosk_app().has_package_name()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
            base::Value(entry.android_kiosk_app().package_name()));
      }
      if (entry.android_kiosk_app().has_class_name()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
            base::Value(entry.android_kiosk_app().class_name()));
      }
      if (entry.android_kiosk_app().has_action()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
            base::Value(entry.android_kiosk_app().action()));
      }
      if (entry.android_kiosk_app().has_display_name()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
            base::Value(entry.android_kiosk_app().display_name()));
      }
      if (entry.web_kiosk_app().has_url()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
            base::Value(entry.web_kiosk_app().url()));
      }
      if (entry.web_kiosk_app().has_title()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
            base::Value(entry.web_kiosk_app().title()));
      }
      if (entry.web_kiosk_app().has_icon_url()) {
        entry_dict.SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
            base::Value(entry.web_kiosk_app().icon_url()));
      }
    } else if (entry.has_deprecated_public_session_id()) {
      // Deprecated public session specification.
      entry_dict.SetKey(kAccountsPrefDeviceLocalAccountsKeyId,
                        base::Value(entry.deprecated_public_session_id()));
      entry_dict.SetKey(
          kAccountsPrefDeviceLocalAccountsKeyType,
          base::Value(
              em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION));
    }
    account_list.push_back(std::move(entry_dict));
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

  if (policy.has_start_up_flags()) {
    std::vector<base::Value> list;
    const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
    const RepeatedPtrField<std::string>& flags = flags_proto.flags();
    for (const std::string& entry : flags) {
      list.push_back(base::Value(entry));
    }
    new_values_cache->SetValue(kStartUpFlags, base::Value(std::move(list)));
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
    std::vector<base::Value> list;
    const em::LoginVideoCaptureAllowedUrlsProto&
        login_video_capture_allowed_urls_proto =
            policy.login_video_capture_allowed_urls();
    for (const auto& value : login_video_capture_allowed_urls_proto.urls()) {
      list.push_back(base::Value(value));
    }
    new_values_cache->SetValue(kLoginVideoCaptureAllowedUrls,
                               base::Value(std::move(list)));
  }

  if (policy.has_device_login_screen_extensions()) {
    std::vector<base::Value> apps;
    const em::DeviceLoginScreenExtensionsProto& proto(
        policy.device_login_screen_extensions());
    for (const auto& app : proto.device_login_screen_extensions()) {
      apps.push_back(base::Value(app));
    }
    new_values_cache->SetValue(kDeviceLoginScreenExtensions,
                               base::Value(std::move(apps)));
  }

  if (policy.has_login_screen_locales()) {
    std::vector<base::Value> locales;
    const em::LoginScreenLocalesProto& login_screen_locales(
        policy.login_screen_locales());
    for (const auto& locale : login_screen_locales.login_screen_locales())
      locales.push_back(base::Value(locale));
    new_values_cache->SetValue(kDeviceLoginScreenLocales,
                               base::Value(std::move(locales)));
  }

  if (policy.has_login_screen_input_methods()) {
    std::vector<base::Value> input_methods;
    const em::LoginScreenInputMethodsProto& login_screen_input_methods(
        policy.login_screen_input_methods());
    for (const auto& input_method :
         login_screen_input_methods.login_screen_input_methods())
      input_methods.push_back(base::Value(input_method));
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

  if (policy.has_saml_login_authentication_type() &&
      policy.saml_login_authentication_type()
          .has_saml_login_authentication_type()) {
    new_values_cache->SetInteger(kSamlLoginAuthenticationType,
                                 policy.saml_login_authentication_type()
                                     .saml_login_authentication_type());
  }

  if (policy.has_device_web_based_attestation_allowed_urls()) {
    const em::StringListPolicyProto& container(
        policy.device_web_based_attestation_allowed_urls());

    base::Value urls(base::Value::Type::LIST);
    for (const std::string& entry : container.value().entries()) {
      urls.Append(entry);
    }

    new_values_cache->SetValue(kDeviceWebBasedAttestationAllowedUrls,
                               std::move(urls));
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
    std::vector<base::Value> list;
    for (int value : allowed_connection_types) {
      list.push_back(base::Value(value));
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
    if (reporting_policy.has_report_boot_mode()) {
      new_values_cache->SetBoolean(kReportDeviceBootMode,
                                   reporting_policy.report_boot_mode());
    }
    if (reporting_policy.has_report_crash_report_info()) {
      new_values_cache->SetBoolean(kReportDeviceCrashReportInfo,
                                   reporting_policy.report_crash_report_info());
    }
    if (reporting_policy.has_report_network_interfaces()) {
      new_values_cache->SetBoolean(
          kReportDeviceNetworkInterfaces,
          reporting_policy.report_network_interfaces());
    }
    if (reporting_policy.has_report_users()) {
      new_values_cache->SetBoolean(kReportDeviceUsers,
                                   reporting_policy.report_users());
    }
    if (reporting_policy.has_report_hardware_status()) {
      new_values_cache->SetBoolean(kReportDeviceHardwareStatus,
                                   reporting_policy.report_hardware_status());
    }
    if (reporting_policy.has_report_session_status()) {
      new_values_cache->SetBoolean(kReportDeviceSessionStatus,
                                   reporting_policy.report_session_status());
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

  new_values_cache->SetBoolean(
      kDeviceAttestationEnabled,
      policy.attestation_settings().attestation_enabled());

  if (policy.has_attestation_settings() &&
      policy.attestation_settings().has_content_protection_enabled()) {
    new_values_cache->SetBoolean(
        kAttestationForContentProtectionEnabled,
        policy.attestation_settings().content_protection_enabled());
  } else {
    new_values_cache->SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

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
                               base::Value(base::Value::Type::DICTIONARY));
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
    if (off_hours_policy)
      new_values_cache->SetValue(
          kDeviceOffHours,
          base::Value::FromUniquePtrValue(std::move(off_hours_policy)));
  }

  if (policy.has_tpm_firmware_update_settings()) {
    new_values_cache->SetValue(kTPMFirmwareUpdateSettings,
                               base::Value::FromUniquePtrValue(
                                   tpm_firmware_update::DecodeSettingsProto(
                                       policy.tpm_firmware_update_settings())));
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

  if (policy.virtual_machines_allowed().has_virtual_machines_allowed()) {
    new_values_cache->SetBoolean(
        kVirtualMachinesAllowed,
        policy.virtual_machines_allowed().virtual_machines_allowed());
  } else {
    // If the policy is missing, default to false on enterprise-enrolled
    // devices.
    if (InstallAttributes::Get()->IsEnterpriseManaged()) {
      new_values_cache->SetBoolean(kVirtualMachinesAllowed, false);
    }
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

  if (policy.has_plugin_vm_license_key()) {
    const em::PluginVmLicenseKeyProto& container(
        policy.plugin_vm_license_key());
    if (container.has_plugin_vm_license_key()) {
      new_values_cache->SetValue(
          kPluginVmLicenseKey, base::Value(container.plugin_vm_license_key()));
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

  // Use Blocklist policy if present, otherwise Blacklist version.
  if (policy.has_device_printers_blocklist()) {
    base::Value list(base::Value::Type::LIST);
    const em::DevicePrintersBlocklistProto& proto(
        policy.device_printers_blocklist());
    for (const auto& id : proto.blocklist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersBlocklist, std::move(list));
  } else if (policy.has_native_device_printers_blacklist()) {
    base::Value list(base::Value::Type::LIST);
    const em::DeviceNativePrintersBlacklistProto& proto(
        policy.native_device_printers_blacklist());
    for (const auto& id : proto.blacklist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersBlocklist, std::move(list));
  }

  // Use Allowlist policy if present, otherwise Whitelist version.
  if (policy.has_device_printers_allowlist()) {
    base::Value list(base::Value::Type::LIST);
    const em::DevicePrintersAllowlistProto& proto(
        policy.device_printers_allowlist());
    for (const auto& id : proto.allowlist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersAllowlist, std::move(list));
  } else if (policy.has_native_device_printers_whitelist()) {
    base::Value list(base::Value::Type::LIST);
    const em::DeviceNativePrintersWhitelistProto& proto(
        policy.native_device_printers_whitelist());
    for (const auto& id : proto.whitelist())
      list.Append(id);
    new_values_cache->SetValue(kDevicePrintersAllowlist, std::move(list));
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

  if (policy.has_device_wilco_dtc_allowed()) {
    const em::DeviceWilcoDtcAllowedProto& container(
        policy.device_wilco_dtc_allowed());
    if (container.has_device_wilco_dtc_allowed()) {
      new_values_cache->SetValue(
          kDeviceWilcoDtcAllowed,
          base::Value(container.device_wilco_dtc_allowed()));
    }
  }

  if (policy.has_device_dock_mac_address_source() &&
      policy.device_dock_mac_address_source().has_source()) {
    new_values_cache->SetInteger(
        kDeviceDockMacAddressSource,
        policy.device_dock_mac_address_source().source());
  }

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
bool DeviceSettingsProvider::IsDeviceSetting(const std::string& name) {
  return base::Contains(kKnownSettings, name);
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
        ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE)) {
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;

    // Revert UI change.
    NotifyObservers(path);
    return;
  }

  if (!IsDeviceSetting(path)) {
    NOTREACHED() << "Try to set unhandled cros setting " << path;
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
    OwnerSettingsServiceChromeOS::UpdateDeviceSettings(path, in_value,
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
  if (new_ownership_status == DeviceSettingsService::OWNERSHIP_TAKEN &&
      ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE &&
      device_settings_service_->HasPrivateOwnerKey()) {
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

    // TODO(https://crbug.com/433840): Some of the above code can be simplified
    // or removed, once the DoSet function is removed - then there will be no
    // pending writes. This is because the only value that needs to be written
    // as a pending write is kStatsReportingPref, and this is now handled by the
    // StatsReportingController - see below.
    // Once DoSet is removed and there are no pending writes that are being
    // maintained by DeviceSettingsProvider, this code for updating the signed
    // settings for the new owner should probably be moved outside of
    // DeviceSettingsProvider.

    StatsReportingController::Get()->OnOwnershipTaken(
        device_settings_service_->GetOwnerSettingsService());
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
  device_settings_.mutable_guest_mode_enabled()->set_guest_mode_enabled(true);
  em::PolicyData empty_policy_data;
  UpdateValuesCache(empty_policy_data, device_settings_, TRUSTED);
  values_cache_.SetBoolean(kPolicyMissingMitigationMode, true);

  return true;
}

const base::Value* DeviceSettingsProvider::Get(const std::string& path) const {
  if (IsDeviceSetting(path)) {
    const base::Value* value;
    if (values_cache_.GetValue(path, &value))
      return value;
  } else {
    NOTREACHED() << "Trying to get non cros setting.";
  }

  return NULL;
}

DeviceSettingsProvider::TrustedStatus
DeviceSettingsProvider::PrepareTrustedValues(base::OnceClosure* callback) {
  TrustedStatus status = RequestTrustedEntity();
  if (status == TEMPORARILY_UNTRUSTED && *callback)
    callbacks_.push_back(std::move(*callback));
  return status;
}

bool DeviceSettingsProvider::HandlesSetting(const std::string& path) const {
  return IsDeviceSetting(path);
}

DeviceSettingsProvider::TrustedStatus
DeviceSettingsProvider::RequestTrustedEntity() {
  if (ownership_status_ == DeviceSettingsService::OWNERSHIP_NONE)
    return TRUSTED;
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
      FALLTHROUGH;
    case DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      VLOG(1) << "No policies present yet, will use the temp storage.";
      trusted_status_ = PERMANENTLY_UNTRUSTED;
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

}  // namespace chromeos
