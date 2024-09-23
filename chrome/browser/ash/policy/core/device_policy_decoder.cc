// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_decoder.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/policy/handlers/device_dlc_predownload_list_policy_handler.h"
#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

using ::google::protobuf::RepeatedPtrField;

namespace em = ::enterprise_management;

// A pattern for validating hostnames.
const char hostNameRegex[] = "^([A-z0-9][A-z0-9-]*\\.)+[A-z0-9]+$";

namespace {

void SetJsonDevicePolicyWithError(
    const std::string& policy_name,
    const std::string& json_string,
    std::unique_ptr<ExternalDataFetcher> external_data_fetcher,
    PolicyMap* policies,
    std::string error) {
  policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::Value(json_string),
                std::move(external_data_fetcher));

  policies->AddMessage(policy_name, PolicyMap::MessageType::kError,
                       IDS_POLICY_PROTO_PARSING_ERROR,
                       {base::UTF8ToUTF16(error)});
}

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
  if (auto result = DecodeJsonStringAndNormalize(json_string, policy_name);
      result.has_value()) {
    policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, std::move(result->decoded_json),
                  std::move(external_data_fetcher));

    if (result->non_fatal_errors.has_value()) {
      policies->AddMessage(policy_name, PolicyMap::MessageType::kError,
                           IDS_POLICY_PROTO_PARSING_ERROR,
                           {base::UTF8ToUTF16(*result->non_fatal_errors)});
    }
  } else {
    SetJsonDevicePolicyWithError(policy_name, json_string,
                                 std::move(external_data_fetcher), policies,
                                 result.error());
  }
}

void SetDeviceDlcPredownloadListPolicy(
    const RepeatedPtrField<std::string>& raw_policy_value,
    PolicyMap* policies) {
  std::string warning;
  base::Value::List decoded_dlc_list =
      policy::DeviceDlcPredownloadListPolicyHandler::
          DecodeDeviceDlcPredownloadListPolicy(raw_policy_value, warning);
  policies->Set(key::kDeviceDlcPredownloadList, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                base::Value(std::move(decoded_dlc_list)), nullptr);
  if (!warning.empty()) {
    policies->AddMessage(
        key::kDeviceDlcPredownloadList, PolicyMap::MessageType::kWarning,
        IDS_POLICY_PROTO_PARSING_ERROR, {base::UTF8ToUTF16(warning)});
  }
}
// Returns a `PolicyLevel` if the policy has been set at that level. If the
// policy has been set at the level of `PolicyOptions::UNSET` returns
// `std::nullopt` instead.
std::optional<PolicyLevel> GetPolicyLevel(
    bool has_policy_options,
    const em::PolicyOptions& policy_option_proto) {
  if (!has_policy_options) {
    return POLICY_LEVEL_MANDATORY;
  }
  switch (policy_option_proto.mode()) {
    case em::PolicyOptions::MANDATORY:
      return POLICY_LEVEL_MANDATORY;
    case em::PolicyOptions::RECOMMENDED:
      return POLICY_LEVEL_RECOMMENDED;
    case em::PolicyOptions::UNSET:
      return std::nullopt;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
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
  if (!RE2::FullMatch(policy_value, pattern)) {
    policies->AddMessage(policy_name, PolicyMap::MessageType::kError,
                         IDS_POLICY_INVALID_VALUE);
  }
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

// Decodes a protobuf integer to an IntegerValue. Returns nullopt in case the
// input value is out of bounds.
std::optional<base::Value> DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return std::nullopt;
  }

  return base::Value(static_cast<int>(value));
}

std::optional<base::Value> DecodeConnectionType(int value) {
  static constexpr auto kConnectionTypes =
      base::MakeFixedFlatMap<int, std::string_view>({
          {em::AutoUpdateSettingsProto::CONNECTION_TYPE_ETHERNET,
           shill::kTypeEthernet},
          {em::AutoUpdateSettingsProto::CONNECTION_TYPE_WIFI, shill::kTypeWifi},
          {em::AutoUpdateSettingsProto::CONNECTION_TYPE_CELLULAR,
           shill::kTypeCellular},
      });
  const auto iter = kConnectionTypes.find(value);
  if (iter == kConnectionTypes.end()) {
    return std::nullopt;
  }
  return base::Value(iter->second);
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
      if (auto value = DecodeIntegerValue(container.value())) {
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
      auto policy_level = GetPolicyLevel(container.has_policy_options(),
                                         container.policy_options());
      if (policy_level) {
        policies->Set(key::kDeviceLoginScreenPrimaryMouseButtonSwitch,
                      policy_level.value(), POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, base::Value(container.value()),
                      nullptr);
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
    base::Value::List allowlist;
    for (const auto& entry : container.user_allowlist()) {
      allowlist.Append(entry);
    }
    policies->Set(key::kDeviceUserAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(allowlist)), nullptr);
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
      if (auto value =
              DecodeIntegerValue(container.login_authentication_behavior())) {
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
    base::Value::List urls;
    for (const auto& entry : container.urls()) {
      urls.Append(entry);
    }
    policies->Set(key::kLoginVideoCaptureAllowedUrls, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(urls)), nullptr);
  }

  if (policy.has_device_login_screen_extensions()) {
    const em::DeviceLoginScreenExtensionsProto& proto(
        policy.device_login_screen_extensions());
    base::Value::List apps;
    for (const auto& app : proto.device_login_screen_extensions()) {
      apps.Append(app);
    }
    policies->Set(key::kDeviceLoginScreenExtensions, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(apps)), nullptr);
  }

  if (policy.has_login_screen_extension_manifest_v2_availability()) {
    const em::LoginScreenExtensionManifestV2AvailabilityProto& proto(
        policy.login_screen_extension_manifest_v2_availability());
    policies->Set(
        key::kDeviceLoginScreenExtensionManifestV2Availability,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
        base::Value(proto.login_screen_extension_manifest_v2_availability()),
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
    base::Value::List locales;
    const em::LoginScreenLocalesProto& login_screen_locales(
        policy.login_screen_locales());
    for (const auto& locale : login_screen_locales.login_screen_locales()) {
      locales.Append(locale);
    }
    policies->Set(key::kDeviceLoginScreenLocales, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(locales)), nullptr);
  }

  if (policy.has_login_screen_input_methods()) {
    base::Value::List input_methods;
    const em::LoginScreenInputMethodsProto& login_screen_input_methods(
        policy.login_screen_input_methods());
    for (const auto& input_method :
         login_screen_input_methods.login_screen_input_methods()) {
      input_methods.Append(input_method);
    }
    policies->Set(key::kDeviceLoginScreenInputMethods, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(input_methods)), nullptr);
  }

  if (policy.has_device_login_screen_auto_select_certificate_for_urls()) {
    base::Value::List rules;
    const em::DeviceLoginScreenAutoSelectCertificateForUrls& proto_rules(
        policy.device_login_screen_auto_select_certificate_for_urls());
    for (const auto& rule :
         proto_rules.login_screen_auto_select_certificate_rules()) {
      rules.Append(rule);
    }
    policies->Set(key::kDeviceLoginScreenAutoSelectCertificateForUrls,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(rules)), nullptr);
  }

  if (policy.has_device_login_screen_webhid_allow_devices_for_urls()) {
    const em::StringPolicyProto& container(
        policy.device_login_screen_webhid_allow_devices_for_urls());
    if (container.has_value()) {
      SetJsonDevicePolicy(key::kDeviceLoginScreenWebHidAllowDevicesForUrls,
                          container.value(), policies);
    }
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
      auto policy_level = GetPolicyLevel(container.has_policy_options(),
                                         container.policy_options());
      if (policy_level) {
        policies->Set(key::kDeviceLoginScreenSystemInfoEnforced,
                      policy_level.value(), POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, base::Value(container.value()),
                      nullptr);
      }
    }
  }

  if (policy.has_device_show_numeric_keyboard_for_password()) {
    const em::BooleanPolicyProto& container(
        policy.device_show_numeric_keyboard_for_password());
    if (container.has_value()) {
      auto policy_level = GetPolicyLevel(container.has_policy_options(),
                                         container.policy_options());
      if (policy_level) {
        policies->Set(key::kDeviceShowNumericKeyboardForPassword,
                      policy_level.value(), POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, base::Value(container.value()),
                      nullptr);
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

    auto policy_level = GetPolicyLevel(container.has_policy_options(),
                                       container.policy_options());
    if (policy_level) {
      base::Value::List urls;

      if (container.has_value()) {
        for (const std::string& entry : container.value().entries()) {
          urls.Append(entry);
        }
      }

      policies->Set(key::kDeviceWebBasedAttestationAllowedUrls,
                    policy_level.value(), POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(std::move(urls)), nullptr);
    }
  }

  if (policy.has_device_login_screen_context_aware_access_signals_allowlist()) {
    const em::StringListPolicyProto& container(
        policy.device_login_screen_context_aware_access_signals_allowlist());
    base::Value::List allowlist;
    if (container.has_value()) {
      for (const std::string& entry : container.value().entries()) {
        allowlist.Append(entry);
      }
    }

    policies->Set(key::kDeviceLoginScreenContextAwareAccessSignalsAllowlist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(allowlist)),
                  nullptr);
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

  if (policy.has_device_screensaver_login_screen_enabled()) {
    const em::DeviceScreensaverLoginScreenEnabledProto& container(
        policy.device_screensaver_login_screen_enabled());
    if (container.has_device_screensaver_login_screen_enabled()) {
      policies->Set(
          key::kDeviceScreensaverLoginScreenEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.device_screensaver_login_screen_enabled()),
          nullptr);
    }
  }

  if (policy.has_device_screensaver_login_screen_idle_timeout_seconds()) {
    const em::DeviceScreensaverLoginScreenIdleTimeoutSecondsProto& container(
        policy.device_screensaver_login_screen_idle_timeout_seconds());
    if (container.has_device_screensaver_login_screen_idle_timeout_seconds()) {
      if (auto value = DecodeIntegerValue(
              container
                  .device_screensaver_login_screen_idle_timeout_seconds())) {
        policies->Set(key::kDeviceScreensaverLoginScreenIdleTimeoutSeconds,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }

  if (policy
          .has_device_screensaver_login_screen_image_display_interval_seconds()) {
    const em::DeviceScreensaverLoginScreenImageDisplayIntervalSecondsProto&
        container(
            policy
                .device_screensaver_login_screen_image_display_interval_seconds());
    if (container
            .has_device_screensaver_login_screen_image_display_interval_seconds()) {
      if (auto value = DecodeIntegerValue(
              container
                  .device_screensaver_login_screen_image_display_interval_seconds())) {
        policies->Set(
            key::kDeviceScreensaverLoginScreenImageDisplayIntervalSeconds,
            POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_screensaver_login_screen_images()) {
    const em::DeviceScreensaverLoginScreenImagesProto& container(
        policy.device_screensaver_login_screen_images());
    base::Value::List image_urls;
    for (const auto& entry :
         container.device_screensaver_login_screen_images()) {
      image_urls.Append(entry);
    }
    policies->Set(key::kDeviceScreensaverLoginScreenImages,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(image_urls)),
                  nullptr);
  }

  if (policy.has_device_authentication_url_blocklist()) {
    const em::StringListPolicyProto& container(
        policy.device_authentication_url_blocklist());

    base::Value::List blocklist;
    if (container.has_value()) {
      for (const auto& entry : container.value().entries()) {
        blocklist.Append(entry);
      }
    }

    policies->Set(key::kDeviceAuthenticationURLBlocklist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(blocklist)),
                  nullptr);
  }

  if (policy.has_device_authentication_url_allowlist()) {
    const em::StringListPolicyProto& container(
        policy.device_authentication_url_allowlist());

    base::Value::List allowlist;
    if (container.has_value()) {
      for (const auto& entry : container.value().entries()) {
        allowlist.Append(entry);
      }
    }

    policies->Set(key::kDeviceAuthenticationURLAllowlist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(allowlist)),
                  nullptr);
  }

  if (policy.has_deviceauthenticationflowautoreloadinterval()) {
    const em::IntegerPolicyProto& container(
        policy.deviceauthenticationflowautoreloadinterval());
    if (container.has_value()) {
      if (auto value = DecodeIntegerValue(container.value())) {
        policies->Set(key::kDeviceAuthenticationFlowAutoReloadInterval,
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
      }
    }
  }
}

base::Value::Dict DecodeDeviceLocalAccountInfoProto(
    const em::DeviceLocalAccountInfoProto& entry) {
  if (!entry.has_type()) {
    if (entry.has_deprecated_public_session_id()) {
      // Deprecated public session specification.
      return base::Value::Dict()
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId,
               entry.deprecated_public_session_id())
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(DeviceLocalAccountType::kPublicSession));
    } else {
      return base::Value::Dict();
    }
  }

  base::Value::Dict entry_dict;
  if (entry.has_account_id()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyId,
                   entry.account_id());
  }
  entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                 static_cast<int>(entry.type()));
  if (entry.kiosk_app().has_app_id()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                   entry.kiosk_app().app_id());
  }
  if (entry.kiosk_app().has_update_url()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                   entry.kiosk_app().update_url());
  }
  if (entry.web_kiosk_app().has_url()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl,
                   entry.web_kiosk_app().url());
  }
  if (entry.web_kiosk_app().has_title()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle,
                   entry.web_kiosk_app().title());
  }
  if (entry.web_kiosk_app().has_icon_url()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl,
                   entry.web_kiosk_app().icon_url());
  }
  if (entry.has_ephemeral_mode()) {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                   static_cast<int>(entry.ephemeral_mode()));
  } else {
    entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                   static_cast<int>(
                       em::DeviceLocalAccountInfoProto::EPHEMERAL_MODE_UNSET));
  }
  if (entry.has_isolated_kiosk_app()) {
    if (entry.isolated_kiosk_app().has_web_bundle_id()) {
      entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
                     entry.isolated_kiosk_app().web_bundle_id());
    }
    if (entry.isolated_kiosk_app().has_update_manifest_url()) {
      entry_dict.Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
                     entry.isolated_kiosk_app().update_manifest_url());
    }
  }
  return entry_dict;
}

void DecodeDeviceLocalAccountsPolicy(
    const em::ChromeDeviceSettingsProto& policy,
    PolicyMap* policies) {
  if (!policy.has_device_local_accounts()) {
    return;
  }
  const em::DeviceLocalAccountsProto& container(policy.device_local_accounts());
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      container.account();
  base::Value::List account_list;
  for (const auto& entry : accounts) {
    account_list.Append(DecodeDeviceLocalAccountInfoProto(entry));
  }

  policies->Set(key::kDeviceLocalAccounts, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                base::Value(std::move(account_list)), nullptr);
  if (container.has_auto_login_id()) {
    policies->Set(key::kDeviceLocalAccountAutoLoginId, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(container.auto_login_id()), nullptr);
  }
  if (container.has_auto_login_delay()) {
    if (auto value = DecodeIntegerValue(container.auto_login_delay())) {
      policies->Set(key::kDeviceLocalAccountAutoLoginDelay,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
    }
  }
  if (container.has_enable_auto_login_bailout()) {
    policies->Set(key::kDeviceLocalAccountAutoLoginBailoutEnabled,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD,
                  base::Value(container.enable_auto_login_bailout()), nullptr);
  }
  if (container.has_prompt_for_network_when_offline()) {
    policies->Set(
        key::kDeviceLocalAccountPromptForNetworkWhenOffline,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
        base::Value(container.prompt_for_network_when_offline()), nullptr);
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
    bool enabled = (container.has_enabled()) ? container.enabled() : false;
    uint32_t upload_rate_kbits =
        (container.has_upload_rate_kbits()) ? container.upload_rate_kbits() : 0;
    uint32_t download_rate_kbits = (container.has_download_rate_kbits())
                                       ? container.download_rate_kbits()
                                       : 0;

    auto throttling_status =
        base::Value::Dict()
            .Set("enabled", enabled)
            .Set("upload_rate_kbits", static_cast<int>(upload_rate_kbits))
            .Set("download_rate_kbits", static_cast<int>(download_rate_kbits));
    policies->Set(key::kNetworkThrottlingEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(throttling_status)), nullptr);
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

  if (policy.has_device_ephemeral_network_policies_enabled()) {
    const em::BooleanPolicyProto& container(
        policy.device_ephemeral_network_policies_enabled());
    if (container.has_value()) {
      policies->Set(key::kDeviceEphemeralNetworkPoliciesEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    /*external_data_fetcher=*/nullptr);
    }
  }

  if (policy.has_devicepostquantumkeyagreementenabled()) {
    const em::BooleanPolicyProto& container(
        policy.devicepostquantumkeyagreementenabled());
    if (container.has_value()) {
      policies->Set(key::kDevicePostQuantumKeyAgreementEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }
}

void DecodeIntegerReportingPolicy(PolicyMap* policies,
                                  const std::string& policy_path,
                                  google::protobuf::int64 int_value) {
  if (auto value = DecodeIntegerValue(int_value)) {
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
    if (container.has_report_network_configuration()) {
      policies->Set(
          key::kReportDeviceNetworkConfiguration, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.report_network_configuration()), nullptr);
    }
    if (container.has_report_network_status()) {
      policies->Set(key::kReportDeviceNetworkStatus, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_network_status()), nullptr);
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
    if (container.has_report_runtime_counters()) {
      policies->Set(key::kDeviceReportRuntimeCounters, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_runtime_counters()), nullptr);
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
    if (container.has_device_report_runtime_counters_checking_rate_ms()) {
      DecodeIntegerReportingPolicy(
          policies, key::kDeviceReportRuntimeCountersCheckingRateMs,
          container.device_report_runtime_counters_checking_rate_ms());
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
    if (container.has_device_activity_heartbeat_enabled()) {
      policies->Set(
          key::kDeviceActivityHeartbeatEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.device_activity_heartbeat_enabled()), nullptr);
    }
    if (container.has_device_activity_heartbeat_collection_rate_ms()) {
      DecodeIntegerReportingPolicy(
          policies, key::kDeviceActivityHeartbeatCollectionRateMs,
          container.device_activity_heartbeat_collection_rate_ms());
    }
    if (container.has_report_network_events()) {
      policies->Set(key::kDeviceReportNetworkEvents, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.report_network_events()), nullptr);
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
      if (auto value =
              DecodeIntegerValue(container.scatter_factor_in_seconds())) {
        policies->Set(key::kDeviceUpdateScatterFactor, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }

    if (container.allowed_connection_types_size()) {
      base::Value::List allowed_connection_types;
      for (const auto& entry : container.allowed_connection_types()) {
        if (auto value = DecodeConnectionType(entry)) {
          allowed_connection_types.Append(std::move(*value));
        }
      }
      policies->Set(key::kDeviceUpdateAllowedConnectionTypes,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD,
                    base::Value(std::move(allowed_connection_types)), nullptr);
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

  if (policy.has_deviceextendedautoupdateenabled()) {
    const em::BooleanPolicyProto& container(
        policy.deviceextendedautoupdateenabled());
    if (container.has_value()) {
      policies->Set(key::kDeviceExtendedAutoUpdateEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
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
      policies->Set(
          key::kDeviceLoginScreenDefaultLargeCursorEnabled,
          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          base::Value(container.login_screen_default_large_cursor_enabled()),
          nullptr);
    }

    if (container.has_login_screen_large_cursor_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_large_cursor_enabled_options(),
          container.login_screen_large_cursor_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenLargeCursorEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_large_cursor_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_show_options_in_system_tray_menu_enabled()) {
      auto policy_level = GetPolicyLevel(
          container
              .has_login_screen_show_options_in_system_tray_menu_enabled_options(),
          container
              .login_screen_show_options_in_system_tray_menu_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenShowOptionsInSystemTrayMenu,
            policy_level.value(), POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
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
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_spoken_feedback_enabled_options(),
          container.login_screen_spoken_feedback_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenSpokenFeedbackEnabled, policy_level.value(),
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
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_high_contrast_enabled_options(),
          container.login_screen_high_contrast_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenHighContrastEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_high_contrast_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_shortcuts_enabled()) {
      auto policy_level =
          GetPolicyLevel(container.has_login_screen_shortcuts_enabled_options(),
                         container.login_screen_shortcuts_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenAccessibilityShortcutsEnabled,
            policy_level.value(), POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_shortcuts_enabled()), nullptr);
      }
    }

    if (container.has_login_screen_default_screen_magnifier_type()) {
      if (auto value = DecodeIntegerValue(
              container.login_screen_default_screen_magnifier_type())) {
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
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_virtual_keyboard_enabled_options(),
          container.login_screen_virtual_keyboard_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenVirtualKeyboardEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_virtual_keyboard_enabled()),
            nullptr);
      }
    }

    if (container.has_login_screen_dictation_enabled()) {
      auto policy_level =
          GetPolicyLevel(container.has_login_screen_dictation_enabled_options(),
                         container.login_screen_dictation_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenDictationEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_dictation_enabled()), nullptr);
      }
    }
    if (container.has_login_screen_select_to_speak_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_select_to_speak_enabled_options(),
          container.login_screen_select_to_speak_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenSelectToSpeakEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_select_to_speak_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_cursor_highlight_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_cursor_highlight_enabled_options(),
          container.login_screen_cursor_highlight_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenCursorHighlightEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_cursor_highlight_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_caret_highlight_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_caret_highlight_enabled_options(),
          container.login_screen_caret_highlight_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenCaretHighlightEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_caret_highlight_enabled()),
            nullptr);
      }
    }
    if (container.has_login_screen_mono_audio_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_mono_audio_enabled_options(),
          container.login_screen_mono_audio_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenMonoAudioEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_mono_audio_enabled()), nullptr);
      }
    }
    if (container.has_login_screen_autoclick_enabled()) {
      auto policy_level =
          GetPolicyLevel(container.has_login_screen_autoclick_enabled_options(),
                         container.login_screen_autoclick_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenAutoclickEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_autoclick_enabled()), nullptr);
      }
    }

    if (container.has_login_screen_sticky_keys_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_sticky_keys_enabled_options(),
          container.login_screen_sticky_keys_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenStickyKeysEnabled, policy_level.value(),
            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
            base::Value(container.login_screen_sticky_keys_enabled()), nullptr);
      }
    }

    if (container.has_login_screen_screen_magnifier_type()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_screen_magnifier_type_options(),
          container.login_screen_screen_magnifier_type_options());
      if (policy_level) {
        if (auto value = DecodeIntegerValue(
                container.login_screen_screen_magnifier_type())) {
          policies->Set(key::kDeviceLoginScreenScreenMagnifierType,
                        policy_level.value(), POLICY_SCOPE_MACHINE,
                        POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
        }
      }
    }

    if (container.has_login_screen_keyboard_focus_highlight_enabled()) {
      auto policy_level = GetPolicyLevel(
          container.has_login_screen_keyboard_focus_highlight_enabled_options(),
          container.login_screen_keyboard_focus_highlight_enabled_options());
      if (policy_level) {
        policies->Set(
            key::kDeviceLoginScreenKeyboardFocusHighlightEnabled,
            policy_level.value(), POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
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
}

void DecodeGenericPolicies(const em::ChromeDeviceSettingsProto& policy,
                           PolicyMap* policies) {
  if (policy.has_device_policy_refresh_rate()) {
    const em::DevicePolicyRefreshRateProto& container(
        policy.device_policy_refresh_rate());
    if (container.has_device_policy_refresh_rate()) {
      if (auto value =
              DecodeIntegerValue(container.device_policy_refresh_rate())) {
        policies->Set(key::kDevicePolicyRefreshRate, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_system_aec_enabled()) {
    const em::DeviceSystemAecEnabledProto& container(
        policy.device_system_aec_enabled());
    if (container.has_device_system_aec_enabled()) {
      policies->Set(key::kDeviceSystemAecEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.device_system_aec_enabled()),
                    nullptr);
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

  if (policy.has_device_login_screen_geolocation_access_level() &&
      policy.device_login_screen_geolocation_access_level()
          .has_geolocation_access_level()) {
    if (auto value = DecodeIntegerValue(
            policy.device_login_screen_geolocation_access_level()
                .geolocation_access_level())) {
      policies->Set(key::kDeviceLoginScreenGeolocationAccessLevel,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
    }
  } else {
    // Set policy default to kAllowed if the policy is unset.
    policies->Set(
        key::kDeviceLoginScreenGeolocationAccessLevel, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
        base::Value(enterprise_management::
                        DeviceLoginScreenGeolocationAccessLevelProto::ALLOWED),
        nullptr);
  }

  if (policy.has_system_timezone()) {
    if (policy.system_timezone().has_timezone()) {
      policies->Set(key::kSystemTimezone, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(policy.system_timezone().timezone()), nullptr);
    }

    if (policy.system_timezone().has_timezone_detection_type()) {
      if (auto value = DecodeIntegerValue(
              policy.system_timezone().timezone_detection_type())) {
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

  if (policy.has_device_hindi_inscript_layout_enabled()) {
    const em::DeviceHindiInscriptLayoutEnabledProto& container(
        policy.device_hindi_inscript_layout_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceHindiInscriptLayoutEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
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
      if (auto value = DecodeIntegerValue(container.uptime_limit())) {
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
      if (auto value = DecodeIntegerValue(container.extension_cache_size())) {
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
      if (auto value =
              DecodeIntegerValue(container.display_rotation_default())) {
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
    base::Value::List allowlist;
    for (const auto& entry : container.id()) {
      base::Value::Dict ids;
      if (entry.has_vendor_id()) {
        ids.Set("vid", base::StringPrintf("%04X", entry.vendor_id()));
      }
      if (entry.has_product_id()) {
        ids.Set("pid", base::StringPrintf("%04X", entry.product_id()));
      }
      allowlist.Append(std::move(ids));
    }
    policies->Set(key::kUsbDetachableAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(allowlist)), nullptr);
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
    if (auto value = DecodeIntegerValue(container.mode())) {
      policies->Set(key::kDeviceSecondFactorAuthentication,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, std::move(*value), nullptr);
    }
  }

  if (policy.has_device_off_hours()) {
    auto off_hours_policy =
        off_hours::ConvertOffHoursProtoToValue(policy.device_off_hours());
    if (off_hours_policy) {
      policies->Set(key::kDeviceOffHours, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(std::move(*off_hours_policy)), nullptr);
    }
  }

  if (policy.has_cast_receiver_name()) {
    const em::CastReceiverNameProto& container(policy.cast_receiver_name());
    if (container.has_name()) {
      policies->Set(key::kCastReceiverName, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.name()), nullptr);
    }
  }

  if (policy.has_device_printers_access_mode()) {
    const em::DevicePrintersAccessModeProto& container(
        policy.device_printers_access_mode());
    if (container.has_access_mode()) {
      if (auto value = DecodeIntegerValue(container.access_mode())) {
        policies->Set(key::kDevicePrintersAccessMode, POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                      std::move(*value), nullptr);
      }
    }
  }

  if (policy.has_device_printers_blocklist()) {
    const em::DevicePrintersBlocklistProto& container(
        policy.device_printers_blocklist());
    base::Value::List blocklist;
    for (const auto& entry : container.blocklist()) {
      blocklist.Append(entry);
    }

    policies->Set(key::kDevicePrintersBlocklist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(blocklist)), nullptr);
  }

  if (policy.has_device_printers_allowlist()) {
    const em::DevicePrintersAllowlistProto& container(
        policy.device_printers_allowlist());
    base::Value::List allowlist;
    for (const auto& entry : container.allowlist()) {
      allowlist.Append(entry);
    }

    policies->Set(key::kDevicePrintersAllowlist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(allowlist)), nullptr);
  }

  if (policy.has_external_print_servers_allowlist()) {
    const em::DeviceExternalPrintServersAllowlistProto& container(
        policy.external_print_servers_allowlist());
    base::Value::List allowlist;
    for (const auto& entry : container.allowlist()) {
      allowlist.Append(entry);
    }

    policies->Set(key::kDeviceExternalPrintServersAllowlist,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(allowlist)),
                  nullptr);
  }

  if (policy.has_tpm_firmware_update_settings()) {
    policies->Set(key::kTPMFirmwareUpdateSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  ash::tpm_firmware_update::DecodeSettingsProto(
                      policy.tpm_firmware_update_settings()),
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

  if (policy.has_virtual_machines_allowed()) {
    const em::VirtualMachinesAllowedProto& container(
        policy.virtual_machines_allowed());
    if (container.has_virtual_machines_allowed()) {
      policies->Set(key::kVirtualMachinesAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.virtual_machines_allowed()), nullptr);
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
      if (auto value = DecodeIntegerValue(container.mode())) {
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

  if (policy.has_device_allowed_bluetooth_services()) {
    const em::DeviceAllowedBluetoothServicesProto& container(
        policy.device_allowed_bluetooth_services());
    base::Value::List allowlist;
    for (const auto& entry : container.allowlist()) {
      allowlist.Append(entry);
    }
    policies->Set(key::kDeviceAllowedBluetoothServices, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(std::move(allowlist)), nullptr);
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

  if (policy.has_device_low_battery_sound()) {
    const em::DeviceLowBatterySoundProto& container(
        policy.device_low_battery_sound());
    if (container.has_enabled()) {
      policies->Set(policy::key::kDeviceLowBatterySoundEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_charging_sounds()) {
    const em::DeviceChargingSoundsProto& container(
        policy.device_charging_sounds());
    if (container.has_enabled()) {
      policies->Set(policy::key::kDeviceChargingSoundsEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_switch_function_keys_behavior_enabled()) {
    const em::DeviceSwitchFunctionKeysBehaviorEnabledProto& container(
        policy.device_switch_function_keys_behavior_enabled());
    if (container.has_enabled()) {
      policies->Set(policy::key::kDeviceSwitchFunctionKeysBehaviorEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_device_dlc_predownload_list()) {
    SetDeviceDlcPredownloadListPolicy(
        policy.device_dlc_predownload_list().value().entries(), policies);
  }

  if (policy.has_device_flex_hw_data_for_product_improvement_enabled()) {
    const em::DeviceFlexHwDataForProductImprovementEnabledProto& container(
        policy.device_flex_hw_data_for_product_improvement_enabled());
    if (container.has_enabled()) {
      policies->Set(key::kDeviceFlexHwDataForProductImprovementEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.enabled()),
                    nullptr);
    }
  }

  if (policy.has_devicehardwarevideodecodingenabled()) {
    const em::BooleanPolicyProto& container(
        policy.devicehardwarevideodecodingenabled());
    if (container.has_value()) {
      policies->Set(key::kDeviceHardwareVideoDecodingEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_deviceloginscreentouchvirtualkeyboardenabled()) {
    const em::BooleanPolicyProto& container(
        policy.deviceloginscreentouchvirtualkeyboardenabled());
    if (container.has_value()) {
      policies->Set(key::kTouchVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(container.value()), nullptr);
    }
  }

  if (policy.has_deviceextensionssystemlogenabled()) {
    const em::BooleanPolicyProto& container(
        policy.deviceextensionssystemlogenabled());
    if (container.has_value()) {
      policies->Set(key::kDeviceExtensionsSystemLogEnabled,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_deviceallowenterpriseremoteaccessconnections()) {
    const em::BooleanPolicyProto& container(
        policy.deviceallowenterpriseremoteaccessconnections());
    if (container.has_value()) {
      policies->Set(key::kDeviceAllowEnterpriseRemoteAccessConnections,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_CLOUD, base::Value(container.value()),
                    nullptr);
    }
  }

  if (policy.has_devicerestrictionschedule()) {
    const em::StringPolicyProto& container(policy.devicerestrictionschedule());
    if (container.has_value()) {
      SetJsonDevicePolicy(key::kDeviceRestrictionSchedule, container.value(),
                          policies);
    }
  }
}

// TODO(b/324221325): Move other Kiosk-related policies to this function.
void DecodeKioskPolicies(const em::ChromeDeviceSettingsProto& policy,
                         PolicyMap* policies) {
  if (policy.has_deviceweeklyscheduledsuspend()) {
    const em::StringPolicyProto& container(
        policy.deviceweeklyscheduledsuspend());
    if (container.has_value()) {
      SetJsonDevicePolicy(key::kDeviceWeeklyScheduledSuspend, container.value(),
                          policies);
    }
  }
}

}  // namespace

DecodeJsonResult::DecodeJsonResult(base::Value decoded_json,
                                   std::optional<std::string> non_fatal_errors)
    : decoded_json(std::move(decoded_json)),
      non_fatal_errors(std::move(non_fatal_errors)) {}

DecodeJsonResult::DecodeJsonResult(DecodeJsonResult&& other) = default;
DecodeJsonResult& DecodeJsonResult::operator=(DecodeJsonResult&& other) =
    default;

DecodeJsonResult::~DecodeJsonResult() = default;

base::expected<DecodeJsonResult, DecodeJsonError> DecodeJsonStringAndNormalize(
    const std::string& json_string,
    const std::string& policy_name) {
  ASSIGN_OR_RETURN(auto parsed_json,
                   base::JSONReader::ReadAndReturnValueWithError(
                       json_string, base::JSON_ALLOW_TRAILING_COMMAS),
                   [](base::JSONReader::Error error) {
                     return "Invalid JSON string: " + std::move(error).message;
                   });

  const Schema& schema = GetChromeSchema().GetKnownProperty(policy_name);
  CHECK(schema.valid());

  std::string schema_error;
  PolicyErrorPath error_path;
  bool changed = false;
  if (!schema.Normalize(&parsed_json, SCHEMA_ALLOW_UNKNOWN, &error_path,
                        &schema_error, &changed)) {
    std::ostringstream msg;
    msg << "Invalid policy value: " << schema_error << " (at "
        << (error_path.empty()
                ? policy_name
                : policy::ErrorPathToString(policy_name, error_path))
        << ")";
    return base::unexpected(msg.str());
  }

  if (changed) {
    std::ostringstream msg;
    msg << "Dropped unknown properties: " << schema_error << " (at "
        << (error_path.empty()
                ? policy_name
                : policy::ErrorPathToString(policy_name, error_path))
        << ")";
    return base::ok(DecodeJsonResult(/*decoded_json=*/std::move(parsed_json),
                                     /*non_fatal_errors=*/msg.str()));
  }

  return base::ok(DecodeJsonResult(/*decoded_json=*/std::move(parsed_json),
                                   /*non_fatal_errors=*/std::nullopt));
}

void DecodeDevicePolicy(
    const em::ChromeDeviceSettingsProto& policy,
    base::WeakPtr<ExternalDataManager> external_data_manager,
    PolicyMap* policies) {
  // Decode the various groups of policies.
  DecodeLoginPolicies(policy, policies);
  DecodeDeviceLocalAccountsPolicy(policy, policies);
  DecodeNetworkPolicies(policy, policies);
  DecodeReportingPolicies(policy, policies);
  DecodeAutoUpdatePolicies(policy, policies);
  DecodeAccessibilityPolicies(policy, policies);
  DecodeExternalDataPolicies(policy, external_data_manager, policies);
  DecodeKioskPolicies(policy, policies);
  DecodeGenericPolicies(policy, policies);
}

}  // namespace policy
