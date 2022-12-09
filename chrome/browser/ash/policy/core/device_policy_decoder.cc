// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_decoder.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

using ::google::protobuf::RepeatedPtrField;

namespace em = ::enterprise_management;

// A pattern for validating hostnames.
const char hostNameRegex[] = "^([A-z0-9][A-z0-9-]*\\.)+[A-z0-9]+$";

namespace {

// If the |json_string| can be decoded and validated against the schema
// identified by |policy_name| in policy_templates.json, the policy
// |policy_name| in |policies| will be set to the decoded base::Value.
// Otherwise, the policy will be set to a base::Value of the original
// |json_string|. This way, the faulty value can still be seen in
// chrome://policy along with any errors/warnings.
void SetJsonDevicePolicy(
    const std::string& policy_name,
    const std::string& json_string,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher,
    PolicyMap* policies) {
  std::string error;
  absl::optional<base::Value> decoded_json =
      DecodeJsonStringAndNormalize(json_string, policy_name, &error);
  base::Value value_to_set = decoded_json.has_value()
                                 ? std::move(decoded_json.value())
                                 : base::Value(json_string);
  policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, std::move(value_to_set),
                std::move(external_data_fetcher));
  if (!error.empty())
    policies->AddMessage(policy_name, PolicyMap::MessageType::kError,
                         IDS_POLICY_PROTO_PARSING_ERROR,
                         {base::UTF8ToUTF16(error)});
}

// Returns true and sets |level| to a PolicyLevel if the policy has been set
// at that level. Returns false if the policy has been set at the level of
// PolicyOptions::UNSET.
bool GetPolicyLevel(bool has_policy_options,
                    const em::PolicyOptions& policy_option_proto,
                    PolicyLevel* level) {
  if (!has_policy_options) {
    *level = POLICY_LEVEL_MANDATORY;
    return true;
  }
  switch (policy_option_proto.mode()) {
    case em::PolicyOptions::MANDATORY:
      *level = POLICY_LEVEL_MANDATORY;
      return true;
    case em::PolicyOptions::RECOMMENDED:
      *level = POLICY_LEVEL_RECOMMENDED;
      return true;
    case em::PolicyOptions::UNSET:
      return false;
  }
}

void SetJsonDevicePolicy(const std::string& policy_name,
                         const std::string& json_string,
                         PolicyMap* policies) {
  SetJsonDevicePolicy(policy_name, json_string,
                      /* external_data_fetcher */ nullptr, policies);
}

// Function that sets the policy value and validates it with a regex, adding
// error in case of not matching.
void SetPolicyWithValidatingRegex(const std::string& policy_name,
                                  const std::string& policy_value,
                                  const std::string& pattern,
                                  PolicyMap* policies) {
  policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::Value(policy_value), nullptr);
  if (!RE2::FullMatch(policy_value, pattern))
    policies->AddMessage(policy_name, PolicyMap::MessageType::kError,
                         IDS_POLICY_INVALID_VALUE);
}

void SetExternalDataDevicePolicy(
    const std::string& policy_name,
    const std::string& json_string,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies) {
  SetJsonDevicePolicy(
      policy_name, json_string,
      std::make_unique<ExternalDataFetcher>(external_data_manager, policy_name),
      policies);
}

// Decodes a protobuf integer to an IntegerValue. Returns NULL in case the input
// value is out of bounds.
std::unique_ptr<base::Value> DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return nullptr;
  }

  return std::make_unique<base::Value>(static_cast<int>(value));
}

std::unique_ptr<base::Value> DecodeConnectionType(int value) {
  const std::map<int, std::string> kConnectionTypes = {
      {em::AutoUpdateSettingsProto::CONNECTION_TYPE_ETHERNET,
       shill::kTypeEthernet},
      {em::AutoUpdateSettingsProto::CONNECTION_TYPE_WIFI, shill::kTypeWifi},
      {em::AutoUpdateSettingsProto::CONNECTION_TYPE_CELLULAR,
       shill::kTypeCellular},
  };
  const auto iter = kConnectionTypes.find(value);
  if (iter == kConnectionTypes.end())
    return nullptr;
  return std::make_unique<base::Value>(iter->second);
}

void DecodeLoginPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PolicyMap* policies) {
  if (policy.has_guest_mode_enabled()) {
    const em::GuestModeEnabledProto& container(policy.guest_mode_enabled());
    if (container.has_guest_mode_enabled()) {
      policies->Set(key::kDeviceGuestModeEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.guest_mode_enabled()), nullptr);
    }
  }

  if (policy.has_device_chrome_variations_type()) {
    const em::IntegerPolicyProto& container(
        policy.device_chrome_variations_type());
    if (container.has_value()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(container.value()));
      if (value) {
        policies->Set(key::kDeviceChromeVariations, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_login_screen_primary_mouse_button_switch()) {
    const em::BooleanPolicyProto& container(
        policy.login_screen_primary_mouse_button_switch());
    if (container.has_value()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_policy_options(),
                         container.policy_options(), &level)) {
        policies->Set(key::kDeviceLoginScreenPrimaryMouseButtonSwitch, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.value()), nullptr);
      }
    }
  }

  if (policy.has_reboot_on_shutdown()) {
    const em::RebootOnShutdownProto& container(policy.reboot_on_shutdown());
    if (container.has_reboot_on_shutdown()) {
      policies->Set(key::kDeviceRebootOnShutdown, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.reboot_on_shutdown()), nullptr);
    }
  }

  if (policy.has_show_user_names()) {
    const em::ShowUserNamesOnSigninProto& container(policy.show_user_names());
    if (container.has_show_user_names()) {
      policies->Set(key::kDeviceShowUserNamesOnSignin, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.show_user_names()), nullptr);
    }
  }

  if (policy.has_allow_new_users()) {
    const em::AllowNewUsersProto& container(policy.allow_new_users());
    if (container.has_allow_new_users()) {
      policies->Set(key::kDeviceAllowNewUsers, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.allow_new_users()), nullptr);
    }
  }

  if (policy.has_user_allowlist()) {
    const em::UserAllowlistProto& container(policy.user_allowlist());
    base::Value allowlist(base::Value::Type::LIST);
    for (const auto& entry : container.user_allowlist())
      allowlist.Append(entry);
    policies->Set(key::kDeviceUserAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(allowlist), nullptr);
  }

  if (policy.has_family_link_accounts_allowed()) {
    const em::DeviceFamilyLinkAccountsAllowedProto& container(
        policy.family_link_accounts_allowed());
    if (container.has_family_link_accounts_allowed()) {
      policies->Set(
          key::kDeviceFamilyLinkAccountsAllowed, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.family_link_accounts_allowed()), nullptr);
    }
  }

  if (policy.has_ephemeral_users_enabled()) {
    const em::EphemeralUsersEnabledProto& container(
        policy.ephemeral_users_enabled());
    if (container.has_ephemeral_users_enabled()) {
      policies->Set(key::kDeviceEphemeralUsersEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.ephemeral_users_enabled()), nullptr);
    }
  }

  if (policy.has_device_local_accounts()) {
    const em::DeviceLocalAccountsProto& container(
        policy.device_local_accounts());
    const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
        container.account();
    base::Value account_list(base::Value::Type::LIST);
    for (const auto& entry : accounts) {
      base::Value entry_dict(base::Value::Type::DICTIONARY);
      if (entry.has_type()) {
        if (entry.has_account_id()) {
          entry_dict.SetStringKey(ash::kAccountsPrefDeviceLocalAccountsKeyId,
                                  entry.account_id());
        }
        entry_dict.SetIntKey(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                             entry.type());
        if (entry.kiosk_app().has_app_id()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
              entry.kiosk_app().app_id());
        }
        if (entry.kiosk_app().has_update_url()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
              entry.kiosk_app().update_url());
        }
        if (entry.android_kiosk_app().has_package_name()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
              entry.android_kiosk_app().package_name());
        }
        if (entry.android_kiosk_app().has_class_name()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
              entry.android_kiosk_app().class_name());
        }
        if (entry.android_kiosk_app().has_action()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
              entry.android_kiosk_app().action());
        }
        if (entry.android_kiosk_app().has_display_name()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
              entry.android_kiosk_app().display_name());
        }
        if (entry.web_kiosk_app().has_url()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
              entry.web_kiosk_app().url());
        }
        if (entry.web_kiosk_app().has_title()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
              entry.web_kiosk_app().title());
        }
        if (entry.web_kiosk_app().has_icon_url()) {
          entry_dict.SetStringKey(
              ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
              entry.web_kiosk_app().icon_url());
        }

      } else if (entry.has_deprecated_public_session_id()) {
        // Deprecated public session specification.
        entry_dict.SetStringKey(ash::kAccountsPrefDeviceLocalAccountsKeyId,
                                entry.deprecated_public_session_id());
        entry_dict.SetIntKey(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                             DeviceLocalAccount::TYPE_PUBLIC_SESSION);
      }
      account_list.Append(std::move(entry_dict));
    }
    policies->Set(key::kDeviceLocalAccounts, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(account_list), nullptr);
    if (container.has_auto_login_id()) {
      policies->Set(key::kDeviceLocalAccountAutoLoginId, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.auto_login_id()), nullptr);
    }
    if (container.has_auto_login_delay()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.auto_login_delay()));
      if (value) {
        policies->Set(key::kDeviceLocalAccountAutoLoginDelay,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
    if (container.has_enable_auto_login_bailout()) {
      policies->Set(
          key::kDeviceLocalAccountAutoLoginBailoutEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.enable_auto_login_bailout()), nullptr);
    }
    if (container.has_prompt_for_network_when_offline()) {
      policies->Set(
          key::kDeviceLocalAccountPromptForNetworkWhenOffline,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.prompt_for_network_when_offline()), nullptr);
    }
  }

  if (policy.has_saml_settings()) {
    const em::SAMLSettingsProto& container(policy.saml_settings());
    if (container.has_transfer_saml_cookies()) {
      policies->Set(key::kDeviceTransferSAMLCookies, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.transfer_saml_cookies()), nullptr);
    }
  }

  if (policy.has_saml_username()) {
    const em::SAMLUsernameProto& container(policy.saml_username());
    if (container.has_url_parameter_to_autofill_saml_username()) {
      policies->Set(
          key::kDeviceAutofillSAMLUsername, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.url_parameter_to_autofill_saml_username()),
          nullptr);
    }
  }

  if (policy.has_login_authentication_behavior()) {
    const em::LoginAuthenticationBehaviorProto& container(
        policy.login_authentication_behavior());
    if (container.has_login_authentication_behavior()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.login_authentication_behavior()));
      if (value) {
        policies->Set(key::kLoginAuthenticationBehavior, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_allow_bluetooth()) {
    const em::AllowBluetoothProto& container(policy.allow_bluetooth());
    if (container.has_allow_bluetooth()) {
      policies->Set(key::kDeviceAllowBluetooth, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.allow_bluetooth()), nullptr);
    }
  }

  if (policy.has_login_video_capture_allowed_urls()) {
    const em::LoginVideoCaptureAllowedUrlsProto& container(
        policy.login_video_capture_allowed_urls());
    base::Value urls(base::Value::Type::LIST);
    for (const auto& entry : container.urls()) {
      urls.Append(entry);
    }
    policies->Set(key::kLoginVideoCaptureAllowedUrls, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(urls),
                  nullptr);
  }

  if (policy.has_device_login_screen_extensions()) {
    const em::DeviceLoginScreenExtensionsProto& proto(
        policy.device_login_screen_extensions());
    base::Value apps(base::Value::Type::LIST);
    for (const auto& app : proto.device_login_screen_extensions()) {
      apps.Append(app);
    }
    policies->Set(key::kDeviceLoginScreenExtensions, POLICY_LEVEL_MANDATORY,
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
    SetPolicyWithValidatingRegex(key::kDeviceLoginScreenDomainAutoComplete,
                                 container.login_screen_domain_auto_complete(),
                                 hostNameRegex, policies);
  }

  if (policy.has_login_screen_locales()) {
    base::Value locales(base::Value::Type::LIST);
    const em::LoginScreenLocalesProto& login_screen_locales(
        policy.login_screen_locales());
    for (const auto& locale : login_screen_locales.login_screen_locales())
      locales.Append(locale);
    policies->Set(key::kDeviceLoginScreenLocales, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(locales),
                  nullptr);
  }

  if (policy.has_login_screen_input_methods()) {
    base::Value input_methods(base::Value::Type::LIST);
    const em::LoginScreenInputMethodsProto& login_screen_input_methods(
        policy.login_screen_input_methods());
    for (const auto& input_method :
         login_screen_input_methods.login_screen_input_methods()) {
      input_methods.Append(input_method);
    }
    policies->Set(key::kDeviceLoginScreenInputMethods, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(input_methods), nullptr);
  }

  if (policy.has_device_login_screen_auto_select_certificate_for_urls()) {
    base::Value rules(base::Value::Type::LIST);
    const em::DeviceLoginScreenAutoSelectCertificateForUrls& proto_rules(
        policy.device_login_screen_auto_select_certificate_for_urls());
    for (const auto& rule :
         proto_rules.login_screen_auto_select_certificate_rules()) {
      rules.Append(rule);
    }
    policies->Set(key::kDeviceLoginScreenAutoSelectCertificateForUrls,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(rules), nullptr);
  }

  if (policy.has_device_login_screen_webusb_allow_devices_for_urls()) {
    const em::DeviceLoginScreenWebUsbAllowDevicesForUrlsProto& container(
        policy.device_login_screen_webusb_allow_devices_for_urls());
    if (container.has_device_login_screen_webusb_allow_devices_for_urls()) {
      SetJsonDevicePolicy(
          key::kDeviceLoginScreenWebUsbAllowDevicesForUrls,
          container.device_login_screen_webusb_allow_devices_for_urls(),
          policies);
    }
  }

  if (policy.has_device_login_screen_system_info_enforced()) {
    const em::BooleanPolicyProto& container(
        policy.device_login_screen_system_info_enforced());
    if (container.has_value()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_policy_options(),
                         container.policy_options(), &level)) {
        policies->Set(key::kDeviceLoginScreenSystemInfoEnforced, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.value()), nullptr);
      }
    }
  }

  if (policy.has_device_show_numeric_keyboard_for_password()) {
    const em::BooleanPolicyProto& container(
        policy.device_show_numeric_keyboard_for_password());
    if (container.has_value()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_policy_options(),
                         container.policy_options(), &level)) {
        policies->Set(key::kDeviceShowNumericKeyboardForPassword, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.value()), nullptr);
      }
    }
  }

  if (policy.has_device_reboot_on_user_signout()) {
    const em::DeviceRebootOnUserSignoutProto& container(
        policy.device_reboot_on_user_signout());
    if (container.has_reboot_on_signout_mode()) {
      policies->Set(key::kDeviceRebootOnUserSignout, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.reboot_on_signout_mode()), nullptr);
    }
  }

  if (policy.has_device_powerwash_allowed()) {
    const em::DevicePowerwashAllowedProto& container(
        policy.device_powerwash_allowed());
    if (container.has_device_powerwash_allowed()) {
      policies->Set(key::kDevicePowerwashAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.device_powerwash_allowed()), nullptr);
    }
  }

  if (policy.has_device_web_based_attestation_allowed_urls()) {
    const em::StringListPolicyProto& container(
        policy.device_web_based_attestation_allowed_urls());

    PolicyLevel level;
    if (GetPolicyLevel(container.has_policy_options(),
                       container.policy_options(), &level)) {
      base::Value urls(base::Value::Type::LIST);

      if (container.has_value()) {
        for (const std::string& entry : container.value().entries()) {
          urls.Append(entry);
        }
      }

      policies->Set(key::kDeviceWebBasedAttestationAllowedUrls, level,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, std::move(urls),
                    nullptr);
    }
  }

  if (policy.has_device_login_screen_context_aware_access_signals_allowlist()) {
    const em::StringListPolicyProto& container(
        policy.device_login_screen_context_aware_access_signals_allowlist());
    base::Value allowlist(base::Value::Type::LIST);
    if (container.has_value()) {
      for (const std::string& entry : container.value().entries()) {
        allowlist.Append(entry);
      }
    }

    policies->Set(key::kDeviceLoginScreenContextAwareAccessSignalsAllowlist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(allowlist), nullptr);
  }

  if (policy.has_required_client_certificate_for_device()) {
    const em::RequiredClientCertificateForDeviceProto& container(
        policy.required_client_certificate_for_device());
    if (container.has_required_client_certificate_for_device()) {
      SetJsonDevicePolicy(key::kRequiredClientCertificateForDevice,
                          container.required_client_certificate_for_device(),
                          policies);
    }
  }

  if (policy.has_managed_guest_session_privacy_warnings()) {
    const em::ManagedGuestSessionPrivacyWarningsProto& container(
        policy.managed_guest_session_privacy_warnings());
    if (container.has_enabled()) {
      policies->Set(key::kManagedGuestSessionPrivacyWarningsEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_login_screen_prompt_on_multiple_matching_certificates()) {
    const em::BooleanPolicyProto& container(
        policy.login_screen_prompt_on_multiple_matching_certificates());
    if (container.has_value()) {
      policies->Set(key::kDeviceLoginScreenPromptOnMultipleMatchingCertificates,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_kiosk_crx_manifest_update_url_ignored()) {
    const em::BooleanPolicyProto& container(
        policy.kiosk_crx_manifest_update_url_ignored());
    if (container.has_value()) {
      policies->Set(key::kKioskCRXManifestUpdateURLIgnored,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }
}

void DecodeNetworkPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_data_roaming_enabled()) {
    const em::DataRoamingEnabledProto& container(policy.data_roaming_enabled());
    if (container.has_data_roaming_enabled()) {
      policies->Set(key::kDeviceDataRoamingEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.data_roaming_enabled()), nullptr);
    }
  }

  if (policy.has_device_wifi_fast_transition_enabled()) {
    const em::DeviceWiFiFastTransitionEnabledProto& container(
        policy.device_wifi_fast_transition_enabled());
    if (container.has_device_wifi_fast_transition_enabled()) {
      policies->Set(
          key::kDeviceWiFiFastTransitionEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.device_wifi_fast_transition_enabled()),
          nullptr);
    }
  }

  if (policy.has_network_throttling()) {
    const em::NetworkThrottlingEnabledProto& container(
        policy.network_throttling());
    base::Value throttling_status(base::Value::Type::DICTIONARY);
    bool enabled = (container.has_enabled()) ? container.enabled() : false;
    uint32_t upload_rate_kbits =
        (container.has_upload_rate_kbits()) ? container.upload_rate_kbits() : 0;
    uint32_t download_rate_kbits = (container.has_download_rate_kbits())
                                       ? container.download_rate_kbits()
                                       : 0;

    throttling_status.SetBoolKey("enabled", enabled);
    throttling_status.SetIntKey("upload_rate_kbits", upload_rate_kbits);
    throttling_status.SetIntKey("download_rate_kbits", download_rate_kbits);
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
                  base::Value(config), nullptr);
  }

  if (policy.has_network_hostname() &&
      policy.network_hostname().has_device_hostname_template()) {
    std::string hostname(policy.network_hostname().device_hostname_template());
    policies->Set(key::kDeviceHostnameTemplate, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(hostname), nullptr);
  }

  if (policy.has_device_kerberos_encryption_types()) {
    const em::DeviceKerberosEncryptionTypesProto& container(
        policy.device_kerberos_encryption_types());
    if (container.has_types()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(container.types()));
      if (value) {
        policies->Set(key::kDeviceKerberosEncryptionTypes,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_system_proxy_settings()) {
    const em::SystemProxySettingsProto& settings_proto(
        policy.system_proxy_settings());
    if (settings_proto.has_system_proxy_settings()) {
      SetJsonDevicePolicy(key::kSystemProxySettings,
                          settings_proto.system_proxy_settings(), policies);
    }
  }

  if (policy.has_device_debug_packet_capture_allowed() &&
      policy.device_debug_packet_capture_allowed().has_allowed()) {
    policies->Set(
        key::kDeviceDebugPacketCaptureAllowed, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
        base::Value(policy.device_debug_packet_capture_allowed().allowed()),
        nullptr);
  }
}

void DecodeIntegerReportingPolicy(PolicyMap* policies,
                                  const std::string& policy_path,
                                  google::protobuf::int64 int_value) {
  std::unique_ptr<base::Value> value = DecodeIntegerValue(int_value);
  if (value) {
    policies->Set(policy_path, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
  }
}

void DecodeReportingPolicies(const em::ChromeDeviceSettingsProto& policy,
                             PolicyMap* policies) {
  if (policy.has_device_reporting()) {
    const em::DeviceReportingProto& container(policy.device_reporting());
    if (container.has_report_version_info()) {
      policies->Set(key::kReportDeviceVersionInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_version_info()), nullptr);
    }
    if (container.has_report_activity_times()) {
      policies->Set(key::kReportDeviceActivityTimes, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_activity_times()), nullptr);
    }
    if (container.has_report_audio_status()) {
      policies->Set(key::kReportDeviceAudioStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_audio_status()), nullptr);
    }
    if (container.has_report_boot_mode()) {
      policies->Set(key::kReportDeviceBootMode, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_boot_mode()), nullptr);
    }
    if (container.has_report_location()) {
      policies->Set(key::kReportDeviceLocation, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_location()), nullptr);
    }
    if (container.has_report_network_configuration()) {
      policies->Set(key::kReportDeviceNetworkConfiguration,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.report_network_configuration()),
                    nullptr);
    }
    if (container.has_report_network_status()) {
      policies->Set(key::kReportDeviceNetworkStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_network_status()),
                    nullptr);
    }
    if (container.has_report_users()) {
      policies->Set(key::kReportDeviceUsers, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_users()), nullptr);
    }
    if (container.has_report_session_status()) {
      policies->Set(key::kReportDeviceSessionStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_session_status()), nullptr);
    }
    if (container.has_report_os_update_status()) {
      policies->Set(key::kReportDeviceOsUpdateStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_os_update_status()), nullptr);
    }
    if (container.has_report_graphics_status()) {
      policies->Set(key::kReportDeviceGraphicsStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_graphics_status()), nullptr);
    }
    if (container.has_report_crash_report_info()) {
      policies->Set(key::kReportDeviceCrashReportInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_crash_report_info()), nullptr);
    }
    if (container.has_report_peripherals()) {
      policies->Set(key::kReportDevicePeripherals, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_peripherals()), nullptr);
    }
    if (container.has_report_power_status()) {
      policies->Set(key::kReportDevicePowerStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_power_status()), nullptr);
    }
    if (container.has_report_storage_status()) {
      policies->Set(key::kReportDeviceStorageStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_storage_status()), nullptr);
    }
    if (container.has_report_board_status()) {
      policies->Set(key::kReportDeviceBoardStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_board_status()), nullptr);
    }
    if (container.has_device_status_frequency()) {
      DecodeIntegerReportingPolicy(policies, key::kReportUploadFrequency,
                                   container.device_status_frequency());
    }
    if (container.has_report_cpu_info()) {
      policies->Set(key::kReportDeviceCpuInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_cpu_info()), nullptr);
    }
    if (container.has_report_timezone_info()) {
      policies->Set(key::kReportDeviceTimezoneInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_timezone_info()), nullptr);
    }
    if (container.has_report_memory_info()) {
      policies->Set(key::kReportDeviceMemoryInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_memory_info()), nullptr);
    }
    if (container.has_report_backlight_info()) {
      policies->Set(key::kReportDeviceBacklightInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_backlight_info()), nullptr);
    }
    if (container.has_report_app_info()) {
      policies->Set(key::kReportDeviceAppInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_app_info()), nullptr);
    }
    if (container.has_report_bluetooth_info()) {
      policies->Set(key::kReportDeviceBluetoothInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_bluetooth_info()), nullptr);
    }
    if (container.has_report_fan_info()) {
      policies->Set(key::kReportDeviceFanInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_fan_info()), nullptr);
    }
    if (container.has_report_vpd_info()) {
      policies->Set(key::kReportDeviceVpdInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_vpd_info()), nullptr);
    }
    if (container.has_report_system_info()) {
      policies->Set(key::kReportDeviceSystemInfo, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_system_info()), nullptr);
    }
    if (container.has_report_security_status()) {
      policies->Set(key::kReportDeviceSecurityStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_security_status()), nullptr);
    }
    if (container.has_report_print_jobs()) {
      policies->Set(key::kReportDevicePrintJobs, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_print_jobs()), nullptr);
    }
    if (container.has_report_login_logout()) {
      policies->Set(key::kReportDeviceLoginLogout, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_login_logout()), nullptr);
    }
    if (container.has_report_crd_sessions()) {
      policies->Set(key::kReportCRDSessions, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_crd_sessions()), nullptr);
    }
    if (container.has_report_network_telemetry_collection_rate_ms()) {
      DecodeIntegerReportingPolicy(
          policies, key::kReportDeviceNetworkTelemetryCollectionRateMs,
          container.report_network_telemetry_collection_rate_ms());
    }
    if (container.has_report_network_telemetry_event_checking_rate_ms()) {
      DecodeIntegerReportingPolicy(
          policies, key::kReportDeviceNetworkTelemetryEventCheckingRateMs,
          container.report_network_telemetry_event_checking_rate_ms());
    }
    if (container.has_report_device_audio_status_checking_rate_ms()) {
      DecodeIntegerReportingPolicy(
          policies, key::kReportDeviceAudioStatusCheckingRateMs,
          container.report_device_audio_status_checking_rate_ms());
    }
    if (container.has_report_signal_strength_event_driven_telemetry()) {
      base::Value::List signal_strength_telemetry_list;
      for (const std::string& telemetry_entry :
           container.report_signal_strength_event_driven_telemetry()
               .entries()) {
        signal_strength_telemetry_list.Append(telemetry_entry);
      }
      policies->Set(
          key::kReportDeviceSignalStrengthEventDrivenTelemetry,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(std::move(signal_strength_telemetry_list)), nullptr);
    }
  }

  if (policy.has_device_heartbeat_settings()) {
    const em::DeviceHeartbeatSettingsProto& container(
        policy.device_heartbeat_settings());
    if (container.has_heartbeat_enabled()) {
      policies->Set(key::kHeartbeatEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.heartbeat_enabled()), nullptr);
    }
    if (container.has_heartbeat_frequency()) {
      DecodeIntegerReportingPolicy(policies, key::kHeartbeatFrequency,
                                   container.heartbeat_frequency());
    }
  }

  if (policy.has_device_log_upload_settings()) {
    const em::DeviceLogUploadSettingsProto& container(
        policy.device_log_upload_settings());
    if (container.has_system_log_upload_enabled()) {
      policies->Set(key::kLogUploadEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.system_log_upload_enabled()),
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
                    base::Value(channel), nullptr);
    }
    if (container.has_release_channel_delegated()) {
      policies->Set(
          key::kChromeOsReleaseChannelDelegated, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.release_channel_delegated()), nullptr);
    }
    if (container.has_release_lts_tag()) {
      policies->Set(key::kDeviceReleaseLtsTag, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.release_lts_tag()), nullptr);
    }
  }

  if (policy.has_auto_update_settings()) {
    const em::AutoUpdateSettingsProto& container(policy.auto_update_settings());
    if (container.has_update_disabled()) {
      policies->Set(key::kDeviceAutoUpdateDisabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.update_disabled()), nullptr);
    }

    if (container.has_target_version_prefix()) {
      policies->Set(key::kDeviceTargetVersionPrefix, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.target_version_prefix()), nullptr);
    }

    // target_version_display_name is not actually a policy, but a display
    // string for target_version_prefix, so we ignore it.

    if (container.has_target_version_selector()) {
      policies->Set(key::kDeviceTargetVersionSelector, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.target_version_selector()), nullptr);
    }

    if (container.has_rollback_to_target_version()) {
      policies->Set(key::kDeviceRollbackToTargetVersion, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.rollback_to_target_version()),
                    nullptr);
    }

    if (container.has_rollback_allowed_milestones()) {
      policies->Set(
          key::kDeviceRollbackAllowedMilestones, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.rollback_allowed_milestones()), nullptr);
    }

    if (container.has_scatter_factor_in_seconds()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.scatter_factor_in_seconds()));
      if (value) {
        policies->Set(key::kDeviceUpdateScatterFactor, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }

    if (container.allowed_connection_types_size()) {
      base::Value allowed_connection_types(base::Value::Type::LIST);
      for (const auto& entry : container.allowed_connection_types()) {
        std::unique_ptr<base::Value> value = DecodeConnectionType(entry);
        if (value)
          allowed_connection_types.Append(std::move(*value));
      }
      policies->Set(key::kDeviceUpdateAllowedConnectionTypes,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(allowed_connection_types),
                    nullptr);
    }

    if (container.has_http_downloads_enabled()) {
      policies->Set(key::kDeviceUpdateHttpDownloadsEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.http_downloads_enabled()), nullptr);
    }

    if (container.has_reboot_after_update()) {
      policies->Set(key::kRebootAfterUpdate, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.reboot_after_update()), nullptr);
    }

    if (container.has_p2p_enabled()) {
      policies->Set(key::kDeviceAutoUpdateP2PEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.p2p_enabled()), nullptr);
    }

    if (container.has_disallowed_time_intervals()) {
      SetJsonDevicePolicy(key::kDeviceAutoUpdateTimeRestrictions,
                          container.disallowed_time_intervals(), policies);
    }

    if (container.has_staging_schedule()) {
      SetJsonDevicePolicy(key::kDeviceUpdateStagingSchedule,
                          container.staging_schedule(), policies);
    }

    if (container.has_device_quick_fix_build_token()) {
      policies->Set(key::kDeviceQuickFixBuildToken, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.device_quick_fix_build_token()),
                    nullptr);
    }

    if (container.has_channel_downgrade_behavior()) {
      policies->Set(
          key::kDeviceChannelDowngradeBehavior, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.channel_downgrade_behavior()), nullptr);
    }
  }

  if (policy.has_allow_kiosk_app_control_chrome_version()) {
    const em::AllowKioskAppControlChromeVersionProto& container(
        policy.allow_kiosk_app_control_chrome_version());
    if (container.has_allow_kiosk_app_control_chrome_version()) {
      policies->Set(
          key::kAllowKioskAppControlChromeVersion, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.allow_kiosk_app_control_chrome_version()),
          nullptr);
    }
  }

  if (policy.has_device_scheduled_update_check()) {
    const em::DeviceScheduledUpdateCheckProto& container(
        policy.device_scheduled_update_check());
    if (container.has_device_scheduled_update_check_settings()) {
      SetJsonDevicePolicy(key::kDeviceScheduledUpdateCheck,
                          container.device_scheduled_update_check_settings(),
                          policies);
    }
  }
}

void DecodeAccessibilityPolicies(const em::ChromeDeviceSettingsProto& policy,
                                 PolicyMap* policies) {
  if (policy.has_accessibility_settings()) {
    const em::AccessibilitySettingsProto& container(
        policy.accessibility_settings());

    if (container.has_login_screen_default_large_cursor_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultLargeCursorEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.login_screen_default_large_cursor_enabled()),
          nullptr);
    }

    if (container.has_login_screen_large_cursor_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_large_cursor_enabled_options(),
              container.login_screen_large_cursor_enabled_options(), &level)) {
        policies->Set(
            key::kDeviceLoginScreenLargeCursorEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_large_cursor_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_show_options_in_system_tray_menu_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container
                  .has_login_screen_show_options_in_system_tray_menu_enabled_options(),
              container
                  .login_screen_show_options_in_system_tray_menu_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenShowOptionsInSystemTrayMenu, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(
                container
                    .login_screen_show_options_in_system_tray_menu_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_default_spoken_feedback_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.login_screen_default_spoken_feedback_enabled()),
          nullptr);
    }

    if (container.has_login_screen_spoken_feedback_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_spoken_feedback_enabled_options(),
              container.login_screen_spoken_feedback_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenSpokenFeedbackEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_spoken_feedback_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_default_high_contrast_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultHighContrastEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.login_screen_default_high_contrast_enabled()),
          nullptr);
    }

    if (container.has_login_screen_high_contrast_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_high_contrast_enabled_options(),
              container.login_screen_high_contrast_enabled_options(), &level)) {
        policies->Set(
            key::kDeviceLoginScreenHighContrastEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_high_contrast_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_shortcuts_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_login_screen_shortcuts_enabled_options(),
                         container.login_screen_shortcuts_enabled_options(),
                         &level)) {
        policies->Set(key::kDeviceLoginScreenAccessibilityShortcutsEnabled,
                      level, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.login_screen_shortcuts_enabled()),
                      nullptr);
      }
    }

    if (container.has_login_screen_default_screen_magnifier_type()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(
          container.login_screen_default_screen_magnifier_type()));
      if (value) {
        policies->Set(key::kDeviceLoginScreenDefaultScreenMagnifierType,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }

    if (container.has_login_screen_default_virtual_keyboard_enabled()) {
      policies->Set(
          key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(
              container.login_screen_default_virtual_keyboard_enabled()),
          nullptr);
    }

    if (container.has_login_screen_virtual_keyboard_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_virtual_keyboard_enabled_options(),
              container.login_screen_virtual_keyboard_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenVirtualKeyboardEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_virtual_keyboard_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_dictation_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_login_screen_dictation_enabled_options(),
                         container.login_screen_dictation_enabled_options(),
                         &level)) {
        policies->Set(key::kDeviceLoginScreenDictationEnabled, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.login_screen_dictation_enabled()),
                      nullptr);
      }
    }
    if (container.has_login_screen_select_to_speak_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_select_to_speak_enabled_options(),
              container.login_screen_select_to_speak_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenSelectToSpeakEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_select_to_speak_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_cursor_highlight_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_cursor_highlight_enabled_options(),
              container.login_screen_cursor_highlight_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenCursorHighlightEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_cursor_highlight_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_caret_highlight_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_caret_highlight_enabled_options(),
              container.login_screen_caret_highlight_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenCaretHighlightEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_caret_highlight_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_mono_audio_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_mono_audio_enabled_options(),
              container.login_screen_mono_audio_enabled_options(), &level)) {
        policies->Set(key::kDeviceLoginScreenMonoAudioEnabled, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.login_screen_mono_audio_enabled()),
                      nullptr);
      }
    }
    if (container.has_login_screen_autoclick_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(container.has_login_screen_autoclick_enabled_options(),
                         container.login_screen_autoclick_enabled_options(),
                         &level)) {
        policies->Set(key::kDeviceLoginScreenAutoclickEnabled, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.login_screen_autoclick_enabled()),
                      nullptr);
      }
    }

    if (container.has_login_screen_sticky_keys_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_sticky_keys_enabled_options(),
              container.login_screen_sticky_keys_enabled_options(), &level)) {
        policies->Set(key::kDeviceLoginScreenStickyKeysEnabled, level,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      base::Value(container.login_screen_sticky_keys_enabled()),
                      nullptr);
      }
    }

    if (container.has_login_screen_screen_magnifier_type()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container.has_login_screen_screen_magnifier_type_options(),
              container.login_screen_screen_magnifier_type_options(), &level)) {
        std::unique_ptr<base::Value> value(
            DecodeIntegerValue(container.login_screen_screen_magnifier_type()));
        if (value) {
          policies->Set(key::kDeviceLoginScreenScreenMagnifierType, level,
                        POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                        std::move(*value), nullptr);
        }
      }
    }

    if (container.has_login_screen_keyboard_focus_highlight_enabled()) {
      PolicyLevel level;
      if (GetPolicyLevel(
              container
                  .has_login_screen_keyboard_focus_highlight_enabled_options(),
              container.login_screen_keyboard_focus_highlight_enabled_options(),
              &level)) {
        policies->Set(
            key::kDeviceLoginScreenKeyboardFocusHighlightEnabled, level,
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(
                container.login_screen_keyboard_focus_highlight_enabled()),
            nullptr);
      }
    }
  }
}

void DecodeExternalDataPolicies(
    const em::ChromeDeviceSettingsProto& policy,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies) {
  if (policy.has_device_wallpaper_image()) {
    const em::DeviceWallpaperImageProto& container(
        policy.device_wallpaper_image());
    if (container.has_device_wallpaper_image()) {
      SetExternalDataDevicePolicy(key::kDeviceWallpaperImage,
                                  container.device_wallpaper_image(),
                                  external_data_manager, policies);
    }
  }

  if (policy.has_device_printers()) {
    const em::DevicePrintersProto& container(policy.device_printers());
    if (container.has_external_policy()) {
      SetExternalDataDevicePolicy(key::kDevicePrinters,
                                  container.external_policy(),
                                  external_data_manager, policies);
    }
  }

  if (policy.has_external_print_servers()) {
    const em::DeviceExternalPrintServersProto& container(
        policy.external_print_servers());
    if (container.has_external_policy()) {
      SetExternalDataDevicePolicy(key::kDeviceExternalPrintServers,
                                  container.external_policy(),
                                  external_data_manager, policies);
    }
  }

  if (policy.has_device_wilco_dtc_configuration()) {
    const em::DeviceWilcoDtcConfigurationProto& container(
        policy.device_wilco_dtc_configuration());
    if (container.has_device_wilco_dtc_configuration()) {
      SetExternalDataDevicePolicy(key::kDeviceWilcoDtcConfiguration,
                                  container.device_wilco_dtc_configuration(),
                                  external_data_manager, policies);
    }
  }
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto& container(
        policy.device_policy_refresh_rate());
    if (container.has_device_policy_refresh_rate()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.device_policy_refresh_rate()));
      if (value) {
        policies->Set(key::kDevicePolicyRefreshRate, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_metrics_enabled()) {
    const em::MetricsEnabledProto& container(policy.metrics_enabled());
    if (container.has_metrics_enabled()) {
      policies->Set(key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.metrics_enabled()), nullptr);
    }
  }

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      policies->Set(key::kSystemTimezone, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(policy.system_timezone().timezone()), nullptr);
    }

    if (policy.system_timezone().has_timezone_detection_type()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(
          policy.system_timezone().timezone_detection_type()));
      if (value) {
        policies->Set(key::kSystemTimezoneAutomaticDetection,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_use_24hour_clock()) {
    if (policy.use_24hour_clock().has_use_24hour_clock()) {
      policies->Set(key::kSystemUse24HourClock, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(policy.use_24hour_clock().use_24hour_clock()),
                    nullptr);
    }
  }

  if (policy.has_keyboard_backlight_color()) {
    const em::KeyboardBacklightColorProto& container(
        policy.keyboard_backlight_color());
    if (container.has_color()) {
      // This policy is interpreted as "Recommended".
      // See the comment at the definition of the
      // ash::prefs::kPersonalizationKeyboardBacklightColor pref (to which this
      // policy will be mapped) for more details.
      policies->Set(key::kDeviceKeyboardBacklightColor,
                    POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.color()),
                    nullptr);
    }
  }

  if (policy.has_allow_redeem_offers()) {
    const em::AllowRedeemChromeOsRegistrationOffersProto& container(
        policy.allow_redeem_offers());
    if (container.has_allow_redeem_offers()) {
      policies->Set(key::kDeviceAllowRedeemChromeOsRegistrationOffers,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.allow_redeem_offers()), nullptr);
    }
  }

  if (policy.has_uptime_limit()) {
    const em::UptimeLimitProto& container(policy.uptime_limit());
    if (container.has_uptime_limit()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.uptime_limit()));
      if (value) {
        policies->Set(key::kUptimeLimit, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_variations_parameter()) {
    if (policy.variations_parameter().has_parameter()) {
      policies->Set(
          key::kDeviceVariationsRestrictParameter, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(policy.variations_parameter().parameter()), nullptr);
    }
  }

  if (policy.has_attestation_settings()) {
    if (policy.attestation_settings().has_attestation_enabled()) {
      policies->Set(
          key::kAttestationEnabledForDevice, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(policy.attestation_settings().attestation_enabled()),
          nullptr);
    }
    if (policy.attestation_settings().has_content_protection_enabled()) {
      policies->Set(
          key::kAttestationForContentProtectionEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(
              policy.attestation_settings().content_protection_enabled()),
          nullptr);
    }
  }

  if (policy.has_system_settings()) {
    const em::SystemSettingsProto& container(policy.system_settings());
    if (container.has_block_devmode()) {
      policies->Set(key::kDeviceBlockDevmode, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.block_devmode()), nullptr);
    }
  }

  if (policy.has_extension_cache_size()) {
    const em::ExtensionCacheSizeProto& container(policy.extension_cache_size());
    if (container.has_extension_cache_size()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.extension_cache_size()));
      if (value) {
        policies->Set(key::kExtensionCacheSize, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_display_rotation_default()) {
    const em::DisplayRotationDefaultProto& container(
        policy.display_rotation_default());
    if (container.has_display_rotation_default()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.display_rotation_default()));
      if (value) {
        policies->Set(key::kDisplayRotationDefault, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_display_resolution()) {
    const em::DeviceDisplayResolutionProto& container(
        policy.device_display_resolution());
    if (container.has_device_display_resolution()) {
      SetJsonDevicePolicy(key::kDeviceDisplayResolution,
                          container.device_display_resolution(), policies);
    }
  }

  if (policy.has_usb_detachable_allowlist()) {
    const em::UsbDetachableAllowlistProto& container(
        policy.usb_detachable_allowlist());
    base::Value allowlist(base::Value::Type::LIST);
    for (const auto& entry : container.id()) {
      base::Value ids(base::Value::Type::DICTIONARY);
      if (entry.has_vendor_id()) {
        ids.SetStringKey("vid", base::StringPrintf("%04X", entry.vendor_id()));
      }
      if (entry.has_product_id()) {
        ids.SetStringKey("pid", base::StringPrintf("%04X", entry.product_id()));
      }
      allowlist.Append(std::move(ids));
    }
    policies->Set(key::kUsbDetachableAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(allowlist), nullptr);
  }

  if (policy.has_quirks_download_enabled()) {
    const em::DeviceQuirksDownloadEnabledProto& container(
        policy.quirks_download_enabled());
    if (container.has_quirks_download_enabled()) {
      policies->Set(key::kDeviceQuirksDownloadEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.quirks_download_enabled()), nullptr);
    }
  }

  if (policy.has_device_second_factor_authentication()) {
    const em::DeviceSecondFactorAuthenticationProto& container(
        policy.device_second_factor_authentication());
    std::unique_ptr<base::Value> value(DecodeIntegerValue(container.mode()));
    if (value) {
      policies->Set(key::kDeviceSecondFactorAuthentication,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
    }
  }

  if (policy.has_device_off_hours()) {
    auto off_hours_policy =
        off_hours::ConvertOffHoursProtoToValue(policy.device_off_hours());
    if (off_hours_policy)
      policies->Set(key::kDeviceOffHours, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(std::move(*off_hours_policy)), nullptr);
  }

  if (policy.has_cast_receiver_name()) {
    const em::CastReceiverNameProto& container(policy.cast_receiver_name());
    if (container.has_name())
      policies->Set(key::kCastReceiverName, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.name()), nullptr);
  }

  if (policy.has_device_printers_access_mode()) {
    const em::DevicePrintersAccessModeProto& container(
        policy.device_printers_access_mode());
    if (container.has_access_mode()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.access_mode()));
      if (value) {
        policies->Set(key::kDevicePrintersAccessMode, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_printers_blocklist()) {
    const em::DevicePrintersBlocklistProto& container(
        policy.device_printers_blocklist());
    base::Value blocklist(base::Value::Type::LIST);
    for (const auto& entry : container.blocklist())
      blocklist.Append(entry);

    policies->Set(key::kDevicePrintersBlocklist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(blocklist), nullptr);
  }

  if (policy.has_device_printers_allowlist()) {
    const em::DevicePrintersAllowlistProto& container(
        policy.device_printers_allowlist());
    base::Value allowlist(base::Value::Type::LIST);
    for (const auto& entry : container.allowlist())
      allowlist.Append(entry);

    policies->Set(key::kDevicePrintersAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(allowlist), nullptr);
  }

  if (policy.has_external_print_servers_allowlist()) {
    const em::DeviceExternalPrintServersAllowlistProto& container(
        policy.external_print_servers_allowlist());
    base::Value allowlist(base::Value::Type::LIST);
    for (const auto& entry : container.allowlist())
      allowlist.Append(entry);

    policies->Set(key::kDeviceExternalPrintServersAllowlist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(allowlist), nullptr);
  }

  if (policy.has_tpm_firmware_update_settings()) {
    policies->Set(key::kTPMFirmwareUpdateSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(*(ash::tpm_firmware_update::DecodeSettingsProto(
                      policy.tpm_firmware_update_settings()))),
                  nullptr);
  }

  if (policy.has_device_minimum_version()) {
    const em::StringPolicyProto& container(policy.device_minimum_version());
    if (container.has_value()) {
      SetJsonDevicePolicy(key::kDeviceMinimumVersion, container.value(),
                          policies);
    }
  }

  if (policy.has_device_minimum_version_aue_message()) {
    const em::StringPolicyProto& container(
        policy.device_minimum_version_aue_message());
    if (container.has_value()) {
      policies->Set(key::kDeviceMinimumVersionAueMessage,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_unaffiliated_arc_allowed()) {
    const em::UnaffiliatedArcAllowedProto& container(
        policy.unaffiliated_arc_allowed());
    if (container.has_unaffiliated_arc_allowed()) {
      policies->Set(key::kUnaffiliatedArcAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.unaffiliated_arc_allowed()), nullptr);
    }
  }

  if (policy.has_device_user_policy_loopback_processing_mode()) {
    const em::DeviceUserPolicyLoopbackProcessingModeProto& container(
        policy.device_user_policy_loopback_processing_mode());
    if (container.has_mode()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(container.mode()));
      if (value) {
        policies->Set(key::kDeviceUserPolicyLoopbackProcessingMode,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_virtual_machines_allowed()) {
    const em::VirtualMachinesAllowedProto& container(
        policy.virtual_machines_allowed());
    if (container.has_virtual_machines_allowed()) {
      policies->Set(key::kVirtualMachinesAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.virtual_machines_allowed()), nullptr);
    }
  }

  if (policy.has_device_machine_password_change_rate()) {
    const em::DeviceMachinePasswordChangeRateProto& container(
        policy.device_machine_password_change_rate());
    if (container.has_rate_days()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.rate_days()));
      if (value) {
        policies->Set(key::kDeviceMachinePasswordChangeRate,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_gpo_cache_lifetime()) {
    const em::DeviceGpoCacheLifetimeProto& container(
        policy.device_gpo_cache_lifetime());
    if (container.has_lifetime_hours()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.lifetime_hours()));
      if (value) {
        policies->Set(key::kDeviceGpoCacheLifetime, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_auth_data_cache_lifetime()) {
    const em::DeviceAuthDataCacheLifetimeProto& container(
        policy.device_auth_data_cache_lifetime());
    if (container.has_lifetime_hours()) {
      std::unique_ptr<base::Value> value(
          DecodeIntegerValue(container.lifetime_hours()));
      if (value) {
        policies->Set(key::kDeviceAuthDataCacheLifetime, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_chromad_to_cloud_migration_enabled()) {
    const em::BooleanPolicyProto& container(
        policy.chromad_to_cloud_migration_enabled());
    if (container.has_value()) {
      policies->Set(key::kChromadToCloudMigrationEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_device_unaffiliated_crostini_allowed()) {
    const em::DeviceUnaffiliatedCrostiniAllowedProto& container(
        policy.device_unaffiliated_crostini_allowed());
    if (container.has_device_unaffiliated_crostini_allowed()) {
      policies->Set(
          key::kDeviceUnaffiliatedCrostiniAllowed, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.device_unaffiliated_crostini_allowed()),
          nullptr);
    }
  }

  if (policy.has_plugin_vm_allowed()) {
    const em::PluginVmAllowedProto& container(policy.plugin_vm_allowed());
    if (container.has_plugin_vm_allowed()) {
      policies->Set(key::kPluginVmAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.plugin_vm_allowed()), nullptr);
    }
  }

  if (policy.has_device_wilco_dtc_allowed() &&
      policy.device_wilco_dtc_allowed().has_device_wilco_dtc_allowed()) {
    VLOG(2) << "Set Wilco DTC allowed to "
            << policy.device_wilco_dtc_allowed().device_wilco_dtc_allowed();
    policies->Set(
        key::kDeviceWilcoDtcAllowed, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
        base::Value(
            policy.device_wilco_dtc_allowed().device_wilco_dtc_allowed()),
        nullptr);
  } else {
    VLOG(2) << "No Wilco DTC allowed policy: "
            << policy.has_device_wilco_dtc_allowed() << " "
            << policy.device_wilco_dtc_allowed().has_device_wilco_dtc_allowed();
  }

  if (policy.has_device_wifi_allowed()) {
    const em::DeviceWiFiAllowedProto& container(policy.device_wifi_allowed());
    if (container.has_device_wifi_allowed()) {
      policies->Set(key::kDeviceWiFiAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.device_wifi_allowed()), nullptr);
    }
  }

  if (policy.has_device_power_peak_shift()) {
    const em::DevicePowerPeakShiftProto& container(
        policy.device_power_peak_shift());
    if (container.has_enabled()) {
      policies->Set(key::kDevicePowerPeakShiftEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.enabled()), nullptr);
    }
    if (container.has_battery_threshold()) {
      policies->Set(key::kDevicePowerPeakShiftBatteryThreshold,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.battery_threshold()), nullptr);
    }
    if (container.has_day_configs()) {
      SetJsonDevicePolicy(key::kDevicePowerPeakShiftDayConfig,
                          container.day_configs(), policies);
    }
  }

  if (policy.has_device_boot_on_ac()) {
    const em::DeviceBootOnAcProto& container(policy.device_boot_on_ac());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceBootOnAcEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.enabled()), nullptr);
    }
  }

  if (policy.has_device_dock_mac_address_source() &&
      policy.device_dock_mac_address_source().has_source()) {
    VLOG(2) << "Set dock MAC address source to "
            << policy.device_dock_mac_address_source().source();
    policies->Set(key::kDeviceDockMacAddressSource, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(policy.device_dock_mac_address_source().source()),
                  nullptr);
  } else {
    VLOG(2) << "No dock MAC address source policy: "
            << policy.has_device_dock_mac_address_source() << " "
            << policy.device_dock_mac_address_source().has_source();
  }

  if (policy.has_device_advanced_battery_charge_mode()) {
    const em::DeviceAdvancedBatteryChargeModeProto& container(
        policy.device_advanced_battery_charge_mode());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceAdvancedBatteryChargeModeEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
    if (container.has_day_configs()) {
      SetJsonDevicePolicy(key::kDeviceAdvancedBatteryChargeModeDayConfig,
                          container.day_configs(), policies);
    }
  }

  if (policy.has_device_battery_charge_mode()) {
    const em::DeviceBatteryChargeModeProto& container(
        policy.device_battery_charge_mode());
    if (container.has_battery_charge_mode()) {
      policies->Set(key::kDeviceBatteryChargeMode, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.battery_charge_mode()), nullptr);
    }
    if (container.has_custom_charge_start()) {
      policies->Set(key::kDeviceBatteryChargeCustomStartCharging,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.custom_charge_start()), nullptr);
    }
    if (container.has_custom_charge_stop()) {
      policies->Set(key::kDeviceBatteryChargeCustomStopCharging,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(container.custom_charge_stop()), nullptr);
    }
  }

  if (policy.has_device_usb_power_share()) {
    const em::DeviceUsbPowerShareProto& container(
        policy.device_usb_power_share());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceUsbPowerShareEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.enabled()), nullptr);
    }
  }

  if (policy.has_device_login_screen_privacy_screen_enabled()) {
    const em::DeviceLoginScreenPrivacyScreenEnabledProto& container(
        policy.device_login_screen_privacy_screen_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceLoginScreenPrivacyScreenEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_crostini_arc_adb_sideloading_allowed()) {
    const em::DeviceCrostiniArcAdbSideloadingAllowedProto& container(
        policy.device_crostini_arc_adb_sideloading_allowed());
    if (container.has_mode()) {
      std::unique_ptr<base::Value> value(DecodeIntegerValue(container.mode()));
      if (value) {
        policies->Set(key::kDeviceCrostiniArcAdbSideloadingAllowed,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_show_low_disk_space_notification()) {
    const em::DeviceShowLowDiskSpaceNotificationProto& container(
        policy.device_show_low_disk_space_notification());
    if (container.has_device_show_low_disk_space_notification()) {
      policies->Set(
          key::kDeviceShowLowDiskSpaceNotification, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.device_show_low_disk_space_notification()),
          nullptr);
    }
  }

  if (policy.has_arc_data_snapshot_hours()) {
    const em::DeviceArcDataSnapshotHoursProto& container(
        policy.arc_data_snapshot_hours());
    if (container.has_arc_data_snapshot_hours()) {
      SetJsonDevicePolicy(key::kDeviceArcDataSnapshotHours,
                          container.arc_data_snapshot_hours(), policies);
    }
  }

  if (policy.has_device_allow_mgs_to_store_display_properties()) {
    const em::BooleanPolicyProto& container(
        policy.device_allow_mgs_to_store_display_properties());
    if (container.has_value()) {
      policies->Set(key::kDeviceAllowMGSToStoreDisplayProperties,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_device_system_wide_tracing_enabled() &&
      policy.device_system_wide_tracing_enabled().has_enabled()) {
    bool enabled = policy.device_system_wide_tracing_enabled().enabled();
    policies->Set(key::kDeviceSystemWideTracingEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(enabled), nullptr);
  } else {
    // Set policy default to false if the policy is unset.
    policies->Set(key::kDeviceSystemWideTracingEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                  base::Value(false), nullptr);
  }

  if (policy.has_device_pci_peripheral_data_access_enabled_v2()) {
    const em::DevicePciPeripheralDataAccessEnabledProtoV2& container(
        policy.device_pci_peripheral_data_access_enabled_v2());
    if (container.has_enabled()) {
      policies->Set(key::kDevicePciPeripheralDataAccessEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_i18n_shortcuts_enabled()) {
    const em::DeviceI18nShortcutsEnabledProto& container(
        policy.device_i18n_shortcuts_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceI18nShortcutsEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.enabled()), nullptr);
    }
  }

  if (policy.has_device_borealis_allowed() &&
      policy.device_borealis_allowed().has_allowed()) {
    policies->Set(key::kDeviceBorealisAllowed, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(policy.device_borealis_allowed().allowed()),
                  nullptr);
  }

  if (policy.has_device_allowed_bluetooth_services()) {
    const em::DeviceAllowedBluetoothServicesProto& container(
        policy.device_allowed_bluetooth_services());
    base::Value allowlist(base::Value::Type::LIST);
    for (const auto& entry : container.allowlist())
      allowlist.Append(entry);
    policies->Set(key::kDeviceAllowedBluetoothServices, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  std::move(allowlist), nullptr);
  }

  if (policy.has_device_scheduled_reboot()) {
    const em::DeviceScheduledRebootProto& container(
        policy.device_scheduled_reboot());
    if (container.has_device_scheduled_reboot_settings()) {
      SetJsonDevicePolicy(key::kDeviceScheduledReboot,
                          container.device_scheduled_reboot_settings(),
                          policies);
    }
  }

  if (policy.has_device_restricted_managed_guest_session_enabled()) {
    const em::DeviceRestrictedManagedGuestSessionEnabledProto& container(
        policy.device_restricted_managed_guest_session_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceRestrictedManagedGuestSessionEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }
  if (policy.has_login_web_ui_lazy_loading()) {
    const em::DeviceLoginScreenWebUILazyLoadingProto& container(
        policy.login_web_ui_lazy_loading());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceLoginScreenWebUILazyLoading,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_keylocker_for_storage_encryption_enabled()) {
    const em::DeviceKeylockerForStorageEncryptionEnabledProto& container(
        policy.keylocker_for_storage_encryption_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceKeylockerForStorageEncryptionEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_run_automatic_cleanup_on_login()) {
    const em::BooleanPolicyProto& container(
        policy.device_run_automatic_cleanup_on_login());
    if (container.has_value()) {
      policies->Set(key::kDeviceRunAutomaticCleanupOnLogin,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_device_encrypted_reporting_pipeline_enabled()) {
    const em::EncryptedReportingPipelineConfigurationProto& container(
        policy.device_encrypted_reporting_pipeline_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceEncryptedReportingPipelineEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_printing_client_name_template()) {
    const em::StringPolicyProto& container(
        policy.device_printing_client_name_template());
    if (container.has_value() && !container.value().empty()) {
      policies->Set(key::kDevicePrintingClientNameTemplate,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_device_report_xdr_events()) {
    const em::DeviceReportXDREventsProto& container(
        policy.device_report_xdr_events());
    if (container.has_enabled()) {
      policies->Set(policy::key::kDeviceReportXDREvents, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.enabled()), nullptr);
    }
  }
}

}  // namespace

absl::optional<base::Value> DecodeJsonStringAndNormalize(
    const std::string& json_string,
    const std::string& policy_name,
    std::string* error) {
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    *error = "Invalid JSON string: " + value_with_error.error().message;
    return absl::nullopt;
  }
  base::Value root = std::move(*value_with_error);

  const Schema& schema = GetChromeSchema().GetKnownProperty(policy_name);
  CHECK(schema.valid());

  std::string schema_error;
  PolicyErrorPath error_path;
  bool changed = false;
  if (!schema.Normalize(&root, SCHEMA_ALLOW_UNKNOWN, &error_path, &schema_error,
                        &changed)) {
    std::ostringstream msg;
    msg << "Invalid policy value: " << schema_error << " (at "
        << (error_path.empty()
                ? policy_name
                : policy::ErrorPathToString(policy_name, error_path))
        << ")";
    *error = msg.str();
    return absl::nullopt;
  }
  if (changed) {
    std::ostringstream msg;
    msg << "Dropped unknown properties: " << schema_error << " (at "
        << (error_path.empty()
                ? policy_name
                : policy::ErrorPathToString(policy_name, error_path))
        << ")";
    *error = msg.str();
  }

  return root;
}

void DecodeDevicePolicy(
    const em::ChromeDeviceSettingsProto& policy,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeNetworkPolicies(policy, policies);
  DecodeReportingPolicies(policy, policies);
  DecodeAutoUpdatePolicies(policy, policies);
  DecodeAccessibilityPolicies(policy, policies);
  DecodeExternalDataPolicies(policy, external_data_manager, policies);
  DecodeGenericPolicies(policy, policies);
}

}  // namespace policy
