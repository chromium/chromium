// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"

#include <limits>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace em = enterprise_management;

namespace policy {

namespace {

// If the |json_string| can be decoded and validated against the schema
// identified by |policy_name| in policy_templates.json, the policy
// |policy_name| in |policies| will be set to the decoded base::Value.
// Otherwise, the policy will be set to a base::Value of the original
// |json_string|. This way, the faulty value can still be seen in
// chrome://policy along with any errors/warnings.
void SetJsonDevicePolicy(const std::string& policy_name,
                         const std::string& json_string,
                         PolicyMap* policies) {
  std::string error;
  std::unique_ptr<base::Value> decoded_json =
      DecodeJsonStringAndNormalize(json_string, policy_name, &error);
  auto value_to_set = decoded_json ? std::move(decoded_json)
                                   : std::make_unique<base::Value>(json_string);
  policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, std::move(value_to_set), nullptr);
  if (!error.empty())
    policies->AddError(policy_name, error);
}

// Decodes a protobuf integer to an IntegerValue. Returns NULL in case the input
// value is out of bounds.
std::unique_ptr<base::Value> DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return std::unique_ptr<base::Value>();
  }

  return std::unique_ptr<base::Value>(new base::Value(static_cast<int>(value)));
}

std::unique_ptr<base::Value> DecodeConnectionType(int value) {
  static const char* const kConnectionTypes[] = {
      shill::kTypeEthernet,  shill::kTypeWifi,     shill::kTypeWimax,
      shill::kTypeBluetooth, shill::kTypeCellular,
  };

  if (value < 0 || value >= static_cast<int>(arraysize(kConnectionTypes)))
    return nullptr;

  return std::make_unique<base::Value>(kConnectionTypes[value]);
}

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PolicyMap* policies) {
  if (policy.has_guest_mode_enabled()) {
    const em::GuestModeEnabledProto& container(policy.guest_mode_enabled());
    if (container.has_guest_mode_enabled()) {
      policies->Set(
          key::kDeviceGuestModeEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.guest_mode_enabled()),
          nullptr);
    }
  }

  if (policy.has_reboot_on_shutdown()) {
    const em::RebootOnShutdownProto& container(policy.reboot_on_shutdown());
    if (container.has_reboot_on_shutdown()) {
      policies->Set(
          key::kDeviceRebootOnShutdown, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.reboot_on_shutdown()),
          nullptr);
    }
  }

  if (policy.has_show_user_names()) {
    const em::ShowUserNamesOnSigninProto& container(policy.show_user_names());
    if (container.has_show_user_names()) {
      policies->Set(key::kDeviceShowUserNamesOnSignin, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.show_user_names()),
                    nullptr);
    }
  }

  if (policy.has_allow_new_users()) {
    const em::AllowNewUsersProto& container(policy.allow_new_users());
    if (container.has_allow_new_users()) {
      policies->Set(key::kDeviceAllowNewUsers, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.allow_new_users()),
                    nullptr);
    }
  }

  if (policy.has_user_whitelist()) {
    const em::UserWhitelistProto& container(policy.user_whitelist());
    std::unique_ptr<base::ListValue> whitelist(new base::ListValue);
    for (const auto& entry : container.user_whitelist())
      whitelist->AppendString(entry);
    policies->Set(key::kDeviceUserWhitelist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(whitelist), nullptr);
  }

  if (policy.has_ephemeral_users_enabled()) {
    const em::EphemeralUsersEnabledProto& container(
        policy.ephemeral_users_enabled());
    if (container.has_ephemeral_users_enabled()) {
      policies->Set(
          key::kDeviceEphemeralUsersEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.ephemeral_users_enabled()),
          nullptr);
    }
  }

  if (policy.has_device_local_accounts()) {
    const em::DeviceLocalAccountsProto& container(
        policy.device_local_accounts());
    const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
        container.account();
    std::unique_ptr<base::ListValue> account_list(new base::ListValue());
    for (const auto& entry : accounts) {
      std::unique_ptr<base::DictionaryValue> entry_dict(
          new base::DictionaryValue());
      if (entry.has_type()) {
        if (entry.has_account_id()) {
          entry_dict->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
                             base::Value(entry.account_id()));
        }
        entry_dict->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyType,
                           base::Value(entry.type()));
        if (entry.kiosk_app().has_app_id()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
              base::Value(entry.kiosk_app().app_id()));
        }
        if (entry.kiosk_app().has_update_url()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
              base::Value(entry.kiosk_app().update_url()));
        }
        if (entry.android_kiosk_app().has_package_name()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
              base::Value(entry.android_kiosk_app().package_name()));
        }
        if (entry.android_kiosk_app().has_class_name()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
              base::Value(entry.android_kiosk_app().class_name()));
        }
        if (entry.android_kiosk_app().has_action()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
              base::Value(entry.android_kiosk_app().action()));
        }
        if (entry.android_kiosk_app().has_display_name()) {
          entry_dict->SetKey(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
              base::Value(entry.android_kiosk_app().display_name()));
        }
      } else if (entry.has_deprecated_public_session_id()) {
        // Deprecated public session specification.
        entry_dict->SetKey(chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
                           base::Value(entry.deprecated_public_session_id()));
        entry_dict->SetKey(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyType,
            base::Value(DeviceLocalAccount::TYPE_PUBLIC_SESSION));
      }
      account_list->Append(std::move(entry_dict));
    }
    policies->Set(key::kDeviceLocalAccounts, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(account_list), nullptr);
    if (container.has_auto_login_id()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginId, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.auto_login_id()),
                    nullptr);
    }
    if (container.has_auto_login_delay()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginDelay,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.auto_login_delay()), nullptr);
    }
    if (container.has_enable_auto_login_bailout()) {
      policies->Set(
          key::kDeviceLocalAccountAutoLoginBailoutEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.enable_auto_login_bailout()),
          nullptr);
    }
    if (container.has_prompt_for_network_when_offline()) {
      policies->Set(key::kDeviceLocalAccountPromptForNetworkWhenOffline,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.prompt_for_network_when_offline()),
                    nullptr);
    }
  }

  if (policy.has_saml_settings()) {
    const em::SAMLSettingsProto& container(policy.saml_settings());
    if (container.has_transfer_saml_cookies()) {
      policies->Set(
          key::kDeviceTransferSAMLCookies, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.transfer_saml_cookies()),
          nullptr);
    }
  }

  if (policy.has_login_authentication_behavior()) {
    const em::LoginAuthenticationBehaviorProto& container(
        policy.login_authentication_behavior());
    if (container.has_login_authentication_behavior()) {
      policies->Set(
          key::kLoginAuthenticationBehavior, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          DecodeIntegerValue(container.login_authentication_behavior()),
          nullptr);
    }
  }

  if (policy.has_allow_bluetooth()) {
    const em::AllowBluetoothProto& container(policy.allow_bluetooth());
    if (container.has_allow_bluetooth()) {
      policies->Set(key::kDeviceAllowBluetooth, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.allow_bluetooth()),
                    nullptr);
    }
  }

  if (policy.has_login_video_capture_allowed_urls()) {
    const em::LoginVideoCaptureAllowedUrlsProto& container(
        policy.login_video_capture_allowed_urls());
    std::unique_ptr<base::ListValue> urls(new base::ListValue());
    for (const auto& entry : container.urls()) {
      urls->AppendString(entry);
    }
    policies->Set(key::kLoginVideoCaptureAllowedUrls, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(urls),
                  nullptr);
  }

  if (policy.has_device_login_screen_app_install_list()) {
    const em::DeviceLoginScreenAppInstallListProto& proto(
        policy.device_login_screen_app_install_list());
    std::unique_ptr<base::ListValue> apps(new base::ListValue);
    for (const auto& app : proto.device_login_screen_app_install_list())
      apps->AppendString(app);
    policies->Set(key::kDeviceLoginScreenAppInstallList, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(apps),
                  nullptr);
  }

  if (policy.has_login_screen_power_management()) {
    const em::LoginScreenPowerManagementProto& container(
        policy.login_screen_power_management());
    if (container.has_login_screen_power_management()) {
      SetJsonDevicePolicy(key::kDeviceLoginScreenPowerManagement,
                          container.login_screen_power_management(), policies);
    }
  }

  if (policy.has_login_screen_domain_auto_complete()) {
    const em::LoginScreenDomainAutoCompleteProto& container(
        policy.login_screen_domain_auto_complete());
    policies->Set(key::kDeviceLoginScreenDomainAutoComplete,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  std::make_unique<base::Value>(
                      container.login_screen_domain_auto_complete()),
                  nullptr);
  }

  if (policy.has_login_screen_locales()) {
    std::unique_ptr<base::ListValue> locales(new base::ListValue);
    const em::LoginScreenLocalesProto& login_screen_locales(
        policy.login_screen_locales());
    for (const auto& locale : login_screen_locales.login_screen_locales())
      locales->AppendString(locale);
    policies->Set(key::kDeviceLoginScreenLocales, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(locales),
                  nullptr);
  }

  if (policy.has_login_screen_input_methods()) {
    std::unique_ptr<base::ListValue> input_methods(new base::ListValue);
    const em::LoginScreenInputMethodsProto& login_screen_input_methods(
        policy.login_screen_input_methods());
    for (const auto& input_method :
         login_screen_input_methods.login_screen_input_methods()) {
      input_methods->AppendString(input_method);
    }
    policies->Set(key::kDeviceLoginScreenInputMethods, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(input_methods), nullptr);
  }

  if (policy.has_device_login_screen_auto_select_certificate_for_urls()) {
    std::unique_ptr<base::ListValue> rules(new base::ListValue);
    const em::DeviceLoginScreenAutoSelectCertificateForUrls& proto_rules(
        policy.device_login_screen_auto_select_certificate_for_urls());
    for (const auto& rule :
         proto_rules.login_screen_auto_select_certificate_rules()) {
      rules->AppendString(rule);
    }
    policies->Set(key::kDeviceLoginScreenAutoSelectCertificateForUrls,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(rules), nullptr);
  }

  if (policy.has_saml_login_authentication_type()) {
    const em::SamlLoginAuthenticationTypeProto& container(
        policy.saml_login_authentication_type());
    if (container.has_saml_login_authentication_type()) {
      policies->Set(key::kDeviceSamlLoginAuthenticationType,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.saml_login_authentication_type()),
                    nullptr);
    }
  }
}

void DecodeNetworkPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_data_roaming_enabled()) {
    const em::DataRoamingEnabledProto& container(policy.data_roaming_enabled());
    if (container.has_data_roaming_enabled()) {
      policies->Set(
          key::kDeviceDataRoamingEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.data_roaming_enabled()),
          nullptr);
    }
  }

  if (policy.has_network_throttling()) {
    const em::NetworkThrottlingEnabledProto& container(
        policy.network_throttling());
    std::unique_ptr<base::DictionaryValue> throttling_status(
        new base::DictionaryValue());
    bool enabled = (container.has_enabled()) ? container.enabled() : false;
    uint32_t upload_rate_kbits =
        (container.has_upload_rate_kbits()) ? container.upload_rate_kbits() : 0;
    uint32_t download_rate_kbits = (container.has_download_rate_kbits())
                                       ? container.download_rate_kbits()
                                       : 0;

    throttling_status->SetBoolean("enabled", enabled);
    throttling_status->SetInteger("upload_rate_kbits", upload_rate_kbits);
    throttling_status->SetInteger("download_rate_kbits", download_rate_kbits);
    policies->Set(key::kNetworkThrottlingEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(throttling_status), nullptr);
  }

  if (policy.has_open_network_configuration() &&
      policy.open_network_configuration().has_open_network_configuration()) {
    std::string config(
        policy.open_network_configuration().open_network_configuration());
    policies->Set(key::kDeviceOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::make_unique<base::Value>(config), nullptr);
  }

  if (policy.has_network_hostname() &&
      policy.network_hostname().has_device_hostname_template()) {
    std::string hostname(policy.network_hostname().device_hostname_template());
    policies->Set(key::kDeviceHostnameTemplate, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::make_unique<base::Value>(hostname), nullptr);
  }

  if (policy.has_device_kerberos_encryption_types()) {
    const em::DeviceKerberosEncryptionTypesProto& container(
        policy.device_kerberos_encryption_types());
    if (container.has_types()) {
      policies->Set(key::kDeviceKerberosEncryptionTypes, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.types()), nullptr);
    }
  }
}

void DecodeReportingPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PolicyMap* policies) {
  if (policy.has_device_reporting()) {
    const em::DeviceReportingProto& container(policy.device_reporting());
    if (container.has_report_version_info()) {
      policies->Set(
          key::kReportDeviceVersionInfo, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.report_version_info()),
          nullptr);
    }
    if (container.has_report_activity_times()) {
      policies->Set(
          key::kReportDeviceActivityTimes, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.report_activity_times()),
          nullptr);
    }
    if (container.has_report_boot_mode()) {
      policies->Set(key::kReportDeviceBootMode, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.report_boot_mode()),
                    nullptr);
    }
    if (container.has_report_location()) {
      policies->Set(key::kReportDeviceLocation, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.report_location()),
                    nullptr);
    }
    if (container.has_report_network_interfaces()) {
      policies->Set(
          key::kReportDeviceNetworkInterfaces, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.report_network_interfaces()),
          nullptr);
    }
    if (container.has_report_users()) {
      policies->Set(key::kReportDeviceUsers, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.report_users()),
                    nullptr);
    }
    if (container.has_report_hardware_status()) {
      policies->Set(
          key::kReportDeviceHardwareStatus, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.report_hardware_status()),
          nullptr);
    }
    if (container.has_report_session_status()) {
      policies->Set(
          key::kReportDeviceSessionStatus, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.report_session_status()),
          nullptr);
    }
    if (container.has_device_status_frequency()) {
      policies->Set(key::kReportUploadFrequency, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.device_status_frequency()),
                    nullptr);
    }
  }

  if (policy.has_device_heartbeat_settings()) {
    const em::DeviceHeartbeatSettingsProto& container(
        policy.device_heartbeat_settings());
    if (container.has_heartbeat_enabled()) {
      policies->Set(
          key::kHeartbeatEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.heartbeat_enabled()),
          nullptr);
    }
    if (container.has_heartbeat_frequency()) {
      policies->Set(key::kHeartbeatFrequency, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.heartbeat_frequency()),
                    nullptr);
    }
  }

  if (policy.has_device_log_upload_settings()) {
    const em::DeviceLogUploadSettingsProto& container(
        policy.device_log_upload_settings());
    if (container.has_system_log_upload_enabled()) {
      policies->Set(
          key::kLogUploadEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.system_log_upload_enabled()),
          nullptr);
    }
  }
}

void DecodeAutoUpdatePolicies(const em::ChromeDeviceSettingsProto& policy,
                              PolicyMap* policies) {
  if (policy.has_release_channel()) {
    const em::ReleaseChannelProto& container(policy.release_channel());
    if (container.has_release_channel()) {
      std::string channel(container.release_channel());
      policies->Set(key::kChromeOsReleaseChannel, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(channel), nullptr);
      // TODO(dubroy): Once http://crosbug.com/17015 is implemented, we won't
      // have to pass the channel in here, only ping the update engine to tell
      // it to fetch the channel from the policy.
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->SetChannel(
          channel, false);
    }
    if (container.has_release_channel_delegated()) {
      policies->Set(
          key::kChromeOsReleaseChannelDelegated, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.release_channel_delegated()),
          nullptr);
    }
  }

  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& container(policy.auto_update_settings());
    if (container.has_update_disabled()) {
      policies->Set(key::kDeviceAutoUpdateDisabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.update_disabled()),
                    nullptr);
    }

    if (container.has_target_version_prefix()) {
      policies->Set(
          key::kDeviceTargetVersionPrefix, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.target_version_prefix()),
          nullptr);
    }

    // target_version_display_name is not actually a policy, but a display
    // string for target_version_prefix, so we ignore it.

    if (container.has_rollback_to_target_version()) {
      policies->Set(
          key::kDeviceRollbackToTargetVersion, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.rollback_to_target_version()),
          nullptr);
    }

    if (container.has_rollback_allowed_milestones()) {
      policies->Set(key::kDeviceRollbackAllowedMilestones,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.rollback_allowed_milestones()),
                    nullptr);
    }

    if (container.has_scatter_factor_in_seconds()) {
      // TODO(dcheng): Shouldn't this use DecodeIntegerValue?
      policies->Set(key::kDeviceUpdateScatterFactor, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(static_cast<int>(
                        container.scatter_factor_in_seconds())),
                    nullptr);
    }

    if (container.allowed_connection_types_size()) {
      std::unique_ptr<base::ListValue> allowed_connection_types(
          new base::ListValue);
      for (const auto& entry : container.allowed_connection_types()) {
        std::unique_ptr<base::Value> value = DecodeConnectionType(entry);
        if (value)
          allowed_connection_types->Append(std::move(value));
      }
      policies->Set(key::kDeviceUpdateAllowedConnectionTypes,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(allowed_connection_types),
                    nullptr);
    }

    if (container.has_http_downloads_enabled()) {
      policies->Set(
          key::kDeviceUpdateHttpDownloadsEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.http_downloads_enabled()),
          nullptr);
    }

    if (container.has_reboot_after_update()) {
      policies->Set(
          key::kRebootAfterUpdate, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.reboot_after_update()),
          nullptr);
    }

    if (container.has_p2p_enabled()) {
      policies->Set(key::kDeviceAutoUpdateP2PEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.p2p_enabled()),
                    nullptr);
    }

    if (container.has_disallowed_time_intervals()) {
      SetJsonDevicePolicy(key::kDeviceAutoUpdateTimeRestrictions,
                          container.disallowed_time_intervals(), policies);
    }

    if (container.has_staging_schedule()) {
      SetJsonDevicePolicy(key::kDeviceUpdateStagingSchedule,
                          container.staging_schedule(), policies);
    }
  }

  if (policy.has_allow_kiosk_app_control_chrome_version()) {
    const em::AllowKioskAppControlChromeVersionProto& container(
        policy.allow_kiosk_app_control_chrome_version());
    if (container.has_allow_kiosk_app_control_chrome_version()) {
      policies->Set(key::kAllowKioskAppControlChromeVersion,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.allow_kiosk_app_control_chrome_version()),
                    nullptr);
    }
  }
}

void DecodeAccessibilityPolicies(const em::ChromeDeviceSettingsProto& policy,
                                 PolicyMap* policies) {
  if (policy.has_accessibility_settings()) {
    const em::AccessibilitySettingsProto& container(
        policy.accessibility_settings());

    if (container.has_login_screen_default_large_cursor_enabled()) {
      policies->Set(key::kDeviceLoginScreenDefaultLargeCursorEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.login_screen_default_large_cursor_enabled()),
                    nullptr);
    }

    if (container.has_login_screen_default_spoken_feedback_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(
              container.login_screen_default_spoken_feedback_enabled()),
          nullptr);
    }

    if (container.has_login_screen_default_high_contrast_enabled()) {
      policies->Set(key::kDeviceLoginScreenDefaultHighContrastEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.login_screen_default_high_contrast_enabled()),
                    nullptr);
    }

    if (container.has_login_screen_default_screen_magnifier_type()) {
      policies->Set(key::kDeviceLoginScreenDefaultScreenMagnifierType,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(
                        container.login_screen_default_screen_magnifier_type()),
                    nullptr);
    }

    if (container.has_login_screen_default_virtual_keyboard_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(
              container.login_screen_default_virtual_keyboard_enabled()),
          nullptr);
    }
  }
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto& container(
        policy.device_policy_refresh_rate());
    if (container.has_device_policy_refresh_rate()) {
      policies->Set(key::kDevicePolicyRefreshRate, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.device_policy_refresh_rate()),
                    nullptr);
    }
  }

  if (policy.has_metrics_enabled()) {
    const em::MetricsEnabledProto& container(policy.metrics_enabled());
    if (container.has_metrics_enabled()) {
      policies->Set(key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.metrics_enabled()),
                    nullptr);
    }
  }

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      policies->Set(
          key::kSystemTimezone, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(policy.system_timezone().timezone()),
          nullptr);
    }

    if (policy.system_timezone().has_timezone_detection_type()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(
          policy.system_timezone().timezone_detection_type()));
      if (value) {
        policies->Set(key::kSystemTimezoneAutomaticDetection,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(value), nullptr);
      }
    }
  }

  if (policy.has_use_24hour_clock()) {
    if (policy.use_24hour_clock().has_use_24hour_clock()) {
      policies->Set(key::kSystemUse24HourClock, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        policy.use_24hour_clock().use_24hour_clock()),
                    nullptr);
    }
  }

  if (policy.has_allow_redeem_offers()) {
    const em::AllowRedeemChromeOsRegistrationOffersProto& container(
        policy.allow_redeem_offers());
    if (container.has_allow_redeem_offers()) {
      policies->Set(
          key::kDeviceAllowRedeemChromeOsRegistrationOffers,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.allow_redeem_offers()),
          nullptr);
    }
  }

  if (policy.has_uptime_limit()) {
    const em::UptimeLimitProto& container(policy.uptime_limit());
    if (container.has_uptime_limit()) {
      policies->Set(key::kUptimeLimit, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.uptime_limit()), nullptr);
    }
  }

  if (policy.has_variations_parameter()) {
    if (policy.variations_parameter().has_parameter()) {
      policies->Set(key::kDeviceVariationsRestrictParameter,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        policy.variations_parameter().parameter()),
                    nullptr);
    }
  }

  if (policy.has_attestation_settings()) {
    if (policy.attestation_settings().has_attestation_enabled()) {
      policies->Set(key::kAttestationEnabledForDevice, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        policy.attestation_settings().attestation_enabled()),
                    nullptr);
    }
    if (policy.attestation_settings().has_content_protection_enabled()) {
      policies->Set(
          key::kAttestationForContentProtectionEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(
              policy.attestation_settings().content_protection_enabled()),
          nullptr);
    }
  }

  if (policy.has_system_settings()) {
    const em::SystemSettingsProto& container(policy.system_settings());
    if (container.has_block_devmode()) {
      policies->Set(key::kDeviceBlockDevmode, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.block_devmode()),
                    nullptr);
    }
  }

  if (policy.has_extension_cache_size()) {
    const em::ExtensionCacheSizeProto& container(policy.extension_cache_size());
    if (container.has_extension_cache_size()) {
      policies->Set(key::kExtensionCacheSize, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.extension_cache_size()),
                    nullptr);
    }
  }

  if (policy.has_display_rotation_default()) {
    const em::DisplayRotationDefaultProto& container(
        policy.display_rotation_default());
    policies->Set(key::kDisplayRotationDefault, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  DecodeIntegerValue(container.display_rotation_default()),
                  nullptr);
  }

  if (policy.has_usb_detachable_whitelist()) {
    const em::UsbDetachableWhitelistProto& container(
        policy.usb_detachable_whitelist());
    std::unique_ptr<base::ListValue> whitelist(new base::ListValue);
    for (const auto& entry : container.id()) {
      std::unique_ptr<base::DictionaryValue> ids(new base::DictionaryValue());
      if (entry.has_vendor_id()) {
        ids->SetString("vid", base::StringPrintf("%04X", entry.vendor_id()));
      }
      if (entry.has_product_id()) {
        ids->SetString("pid", base::StringPrintf("%04X", entry.product_id()));
      }
      whitelist->Append(std::move(ids));
    }
    policies->Set(key::kUsbDetachableWhitelist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(whitelist), nullptr);
  }

  if (policy.has_quirks_download_enabled()) {
    const em::DeviceQuirksDownloadEnabledProto& container(
        policy.quirks_download_enabled());
    if (container.has_quirks_download_enabled()) {
      policies->Set(
          key::kDeviceQuirksDownloadEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.quirks_download_enabled()),
          nullptr);
    }
  }

  if (policy.has_device_wallpaper_image()) {
    const em::DeviceWallpaperImageProto& container(
        policy.device_wallpaper_image());
    if (container.has_device_wallpaper_image()) {
      SetJsonDevicePolicy(key::kDeviceWallpaperImage,
                          container.device_wallpaper_image(), policies);
    }
  }

  if (policy.has_device_second_factor_authentication()) {
    const em::DeviceSecondFactorAuthenticationProto& container(
        policy.device_second_factor_authentication());
    policies->Set(key::kDeviceSecondFactorAuthentication,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, DecodeIntegerValue(container.mode()),
                  nullptr);
  }

  if (policy.has_device_off_hours()) {
    auto off_hours_policy =
        off_hours::ConvertOffHoursProtoToValue(policy.device_off_hours());
    if (off_hours_policy)
      policies->Set(key::kDeviceOffHours, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::move(off_hours_policy), nullptr);
  }

  if (policy.has_cast_receiver_name()) {
    const em::CastReceiverNameProto& container(policy.cast_receiver_name());
    if (container.has_name())
      policies->Set(key::kCastReceiverName, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.name()), nullptr);
  }

  if (policy.has_native_device_printers()) {
    const em::DeviceNativePrintersProto& container(
        policy.native_device_printers());
    if (container.has_external_policy()) {
      SetJsonDevicePolicy(key::kDeviceNativePrinters,
                          container.external_policy(), policies);
    }
  }

  if (policy.has_native_device_printers_access_mode()) {
    const em::DeviceNativePrintersAccessModeProto& container(
        policy.native_device_printers_access_mode());
    if (container.has_access_mode()) {
      policies->Set(key::kDeviceNativePrintersAccessMode,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.access_mode()), nullptr);
    }
  }

  if (policy.has_native_device_printers_blacklist()) {
    const em::DeviceNativePrintersBlacklistProto& container(
        policy.native_device_printers_blacklist());
    std::unique_ptr<base::ListValue> blacklist =
        std::make_unique<base::ListValue>();
    for (const auto& entry : container.blacklist())
      blacklist->AppendString(entry);

    policies->Set(key::kDeviceNativePrintersBlacklist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(blacklist), nullptr);
  }

  if (policy.has_native_device_printers_whitelist()) {
    const em::DeviceNativePrintersWhitelistProto& container(
        policy.native_device_printers_whitelist());
    std::unique_ptr<base::ListValue> whitelist =
        std::make_unique<base::ListValue>();
    for (const auto& entry : container.whitelist())
      whitelist->AppendString(entry);

    policies->Set(key::kDeviceNativePrintersWhitelist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(whitelist), nullptr);
  }

  if (policy.has_tpm_firmware_update_settings()) {
    policies->Set(key::kTPMFirmwareUpdateSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  chromeos::tpm_firmware_update::DecodeSettingsProto(
                      policy.tpm_firmware_update_settings()),
                  nullptr);
  }

  if (policy.has_minimum_required_version()) {
    const em::MinimumRequiredVersionProto& container(
        policy.minimum_required_version());
    if (container.has_chrome_version())
      policies->Set(key::kMinimumRequiredChromeVersion, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(container.chrome_version()),
                    nullptr);
  }

  if (policy.has_unaffiliated_arc_allowed()) {
    const em::UnaffiliatedArcAllowedProto& container(
        policy.unaffiliated_arc_allowed());
    if (container.has_unaffiliated_arc_allowed()) {
      policies->Set(
          key::kUnaffiliatedArcAllowed, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.unaffiliated_arc_allowed()),
          nullptr);
    }
  }

  if (policy.has_device_user_policy_loopback_processing_mode()) {
    const em::DeviceUserPolicyLoopbackProcessingModeProto& container(
        policy.device_user_policy_loopback_processing_mode());
    if (container.has_mode()) {
      policies->Set(key::kDeviceUserPolicyLoopbackProcessingMode,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, DecodeIntegerValue(container.mode()),
                    nullptr);
    }
  }

  if (policy.has_device_login_screen_isolate_origins()) {
    const em::DeviceLoginScreenIsolateOriginsProto& container(
        policy.device_login_screen_isolate_origins());
    if (container.has_isolate_origins()) {
      policies->Set(
          key::kDeviceLoginScreenIsolateOrigins, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.isolate_origins()), nullptr);
    }
  }

  if (policy.has_device_login_screen_site_per_process()) {
    const em::DeviceLoginScreenSitePerProcessProto& container(
        policy.device_login_screen_site_per_process());
    if (container.has_site_per_process()) {
      policies->Set(
          key::kDeviceLoginScreenSitePerProcess, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.site_per_process()), nullptr);
    }
  }

  if (policy.has_virtual_machines_allowed()) {
    const em::VirtualMachinesAllowedProto& container(
        policy.virtual_machines_allowed());
    if (container.has_virtual_machines_allowed()) {
      policies->Set(
          key::kVirtualMachinesAllowed, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(container.virtual_machines_allowed()),
          nullptr);
    }
  }

  if (policy.has_device_machine_password_change_rate()) {
    const em::DeviceMachinePasswordChangeRateProto& container(
        policy.device_machine_password_change_rate());
    if (container.has_rate_days()) {
      policies->Set(key::kDeviceMachinePasswordChangeRate,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    DecodeIntegerValue(container.rate_days()), nullptr);
    }
  }

  if (policy.has_device_unaffiliated_crostini_allowed()) {
    const em::DeviceUnaffiliatedCrostiniAllowedProto& container(
        policy.device_unaffiliated_crostini_allowed());
    if (container.has_device_unaffiliated_crostini_allowed()) {
      policies->Set(key::kDeviceUnaffiliatedCrostiniAllowed,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    std::make_unique<base::Value>(
                        container.device_unaffiliated_crostini_allowed()),
                    nullptr);
    }
  }
}

}  // namespace

std::unique_ptr<base::Value> DecodeJsonStringAndNormalize(
    const std::string& json_string,
    const std::string& policy_name,
    std::string* error) {
  std::string json_error;
  std::unique_ptr<base::Value> root = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &json_error);
  if (!root) {
    *error = "Invalid JSON string: " + json_error;
    return nullptr;
  }

  const Schema& schema =
      policy::GetChromeSchema().GetKnownProperty(policy_name);
  CHECK(schema.valid());

  std::string schema_error;
  std::string error_path;
  bool changed = false;
  if (!schema.Normalize(root.get(), SCHEMA_ALLOW_UNKNOWN, &error_path,
                        &schema_error, &changed)) {
    std::ostringstream msg;
    msg << "Invalid policy value: " << schema_error << " (at "
        << (error_path.empty() ? "toplevel" : error_path) << ")";
    *error = msg.str();
    return nullptr;
  }
  if (changed) {
    std::ostringstream msg;
    msg << "Dropped unknown properties: " << schema_error << " (at "
        << (error_path.empty() ? "toplevel" : error_path) << ")";
    *error = msg.str();
  }

  return root;
}

void DecodeDevicePolicy(const em::ChromeDeviceSettingsProto& policy,
                        PolicyMap* policies) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeNetworkPolicies(policy, policies);
  DecodeReportingPolicies(policy, policies);
  DecodeAutoUpdatePolicies(policy, policies);
  DecodeAccessibilityPolicies(policy, policies);
  DecodeGenericPolicies(policy, policies);
}

}  // namespace policy
