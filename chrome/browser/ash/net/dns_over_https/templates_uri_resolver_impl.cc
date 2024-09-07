// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/sha2.h"

namespace {

constexpr int kMinSaltSize = 8;
constexpr int kMaxSaltSize = 32;
constexpr char kUserEmailPlaceholder[] = "${USER_EMAIL}";
constexpr char kUserEmailDomainPlaceholder[] = "${USER_EMAIL_DOMAIN}";
constexpr char kUserEmailNamePlaceholder[] = "${USER_EMAIL_NAME}";
constexpr char kDeviceDirectoryIdPlaceholder[] = "${DEVICE_DIRECTORY_ID}";
constexpr char kDeviceSerialNumberPlaceholder[] = "${DEVICE_SERIAL_NUMBER}";
constexpr char kDeviceAssetIdPlaceholder[] = "${DEVICE_ASSET_ID}";
constexpr char kDeviceAnnotatedLocationPlaceholder[] =
    "${DEVICE_ANNOTATED_LOCATION}";
constexpr char kDeviceIpPlaceholder[] = "${DEVICE_IP_ADDRESSES}";
constexpr char kPlaceholderStartSymbol[] = "${";
constexpr char kPlaceholderEndSymbol[] = "}";

// Prefix values used to indicate the IP protocol of the IP addresses in the
// effective DoH template URI.
constexpr char kIPv4Prefix[] = "0010";
constexpr char kIPv6Prefix[] = "0020";

// Used as a replacement value for device identifiers when the user is
// unaffiliated.
constexpr char kDeviceNotManaged[] = "VALUE_NOT_AVAILABLE";
constexpr char kIdentifierNotAvailable[] = "${VALUE_NOT_AVAILABLE}";
constexpr char kUnknownPlaceholderMessage[] =
    "Templates contain not replaced placeholder: ";

// Part before "@" of the given |email| address.
// "some_email@domain.com" => "some_email"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailName(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos) {
    return std::string();
  }
  return email.substr(0, at_sign_pos);
}

// Part after "@" of an email address.
// "some_email@domain.com" => "domain.com"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailDomain(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos) {
    return std::string();
  }
  return email.substr(at_sign_pos + 1);
}

// If `hash_variable` is true, the output is the hex encoded result of the
// hashed `salt` + `input` value.  Otherwise we return the input between
// placeholder delimiters.
std::string FormatVariable(const std::string& input,
                           const std::string& salt,
                           bool hash_variable) {
  if (!hash_variable) {
    return "${" + input + "}";
  }
  return base::HexEncode(crypto::SHA256HashString(salt + input));
}

// Returns a hex string representing all IP addresses (IPv4 and/or IPv6)
// associated with the default network. The addresses are hex encoded in network
// byte order. The addresses are prefixed with a string that indicates the
// protocol of the address (`kIPv4Prefix` and `kIPv6Prefix`). For privacy
// reasons, IP replacement in the DoH URI template is only allowed if:
// - The network is managed via user policy.
// - The network is managed via device policy and the user is
// affiliated.
// - The default network is not a VPN.
// If the conditions above are not met or there is no connected network, this
// method returns an empty string.
// There is no separator between addresses if multiple IP addresses are
// returned.
std::string GetIpReplacementValue(bool use_network_byte_order,
                                  const user_manager::User& user) {
  // NetworkHandler may be un-initialized in unit tests.
  if (!ash::NetworkHandler::IsInitialized()) {
    return std::string();
  }
  const ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  if (!network_state_handler) {
    return std::string();
  }

  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  if (!network) {
    return std::string();
  }

  if (network->type() == shill::kTypeVPN) {
    return std::string();
  }

  if (network->onc_source() != ::onc::ONCSource::ONC_SOURCE_USER_POLICY &&
      (!user.IsAffiliated() ||
       network->onc_source() != ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY)) {
    return std::string();
  }

  const ash::DeviceState* device =
      network_state_handler->GetDeviceState(network->device_path());
  if (!device) {
    return std::string();
  }

  std::string replacement;
  net::IPAddress ipv4_address;
  if (ipv4_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv4))) {
    if (use_network_byte_order) {
      replacement = kIPv4Prefix + base::HexEncode(ipv4_address.bytes());
    } else {
      replacement =
          FormatVariable(ipv4_address.ToString(), /*salt=*/std::string(),
                         /*hash_variable=*/false);
    }
  }
  // The default network can have multiple IPv6 addresses. Only the RFC 4941
  // privacy address is relevant, the following code fetches that address.
  net::IPAddress ipv6_address;
  if (ipv6_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv6))) {
    if (use_network_byte_order) {
      replacement += kIPv6Prefix + base::HexEncode(ipv6_address.bytes());
    } else {
      replacement +=
          FormatVariable(ipv6_address.ToString(), /*salt=*/std::string(),
                         /*hash_variable=*/false);
    }
  }
  return replacement;
}

// Returns first found placeholder in `templates` starting from position `pos`,
// as option value. If no placeholders are found, returns empty optional.
std::optional<std::string_view> GetNextPlaceholder(std::string_view templates,
                                                   size_t pos) {
  size_t placeholder_start = templates.find(kPlaceholderStartSymbol, pos);
  if (placeholder_start == std::string::npos) {
    return std::nullopt;
  }

  size_t placeholder_end =
      templates.find(kPlaceholderEndSymbol, placeholder_start);
  if (placeholder_end == std::string::npos) {
    LOG(WARNING) << "Placeholders end symbol is missed in " << templates;
    return std::nullopt;
  }

  return std::make_optional(templates.substr(
      placeholder_start, placeholder_end - placeholder_start + 1));
}

// Checks if display templates `display_str` (with replaced identifiers) and
// original templates `raw_str` contain placeholders with the same values. This
// will indicate that those placeholders were not replaced. Replace those
// placeholders with kIdentifierNotAvailable. Some placeholders like
// "DEVICE_IP_ADDRESSES" can create 2 new placeholders in the display
// template, so searching for every original placeholder instead of fetching
// all of them and comparing one to one.
void HighlightUnknownDisplayPlaceholders(std::string& display_str,
                                         std::string_view raw_str) {
  size_t search_start_pos = 0;
  std::optional<std::string_view> maybe_placeholder =
      GetNextPlaceholder(raw_str, search_start_pos);
  while (maybe_placeholder.has_value()) {
    std::string_view placeholder = maybe_placeholder.value();
    size_t placeholder_pos_in_display = display_str.find(placeholder);
    if (placeholder_pos_in_display != std::string::npos) {
      LOG(WARNING) << kUnknownPlaceholderMessage << placeholder
                   << ", value is not available";
      base::ReplaceSubstringsAfterOffset(&display_str,
                                         placeholder_pos_in_display,
                                         placeholder, kIdentifierNotAvailable);
    }

    search_start_pos =
        raw_str.find(kPlaceholderEndSymbol, search_start_pos + 1);
    maybe_placeholder = GetNextPlaceholder(raw_str, search_start_pos);
  }
}

// Looks into effective `templates` if they still contain placeholders which
// were not replaced with data and strip them off. This step keeps compatibility
// between new type of placeholders delivered by policy and older OS versions
// which still have no definitions for such placeholders.
void StripUnknownEffectivePlaceholders(std::string& templates) {
  size_t search_start_pos = 0;

  std::optional<std::string_view> maybe_placeholder =
      GetNextPlaceholder(templates, search_start_pos);
  while (maybe_placeholder.has_value()) {
    std::string placeholder(maybe_placeholder.value());
    LOG(WARNING) << kUnknownPlaceholderMessage << placeholder
                 << ", it will be deleted";
    search_start_pos =
        templates.find(kPlaceholderStartSymbol, search_start_pos);
    base::ReplaceSubstringsAfterOffset(&templates, search_start_pos,
                                       placeholder, "");
    maybe_placeholder = GetNextPlaceholder(templates, search_start_pos);
  }
}

// Returns a copy of `template` where the identifier placeholders are replaced
// with real user and device data.
// If `hash_variable` is true, then the user and device identifiers are hashed
// with `salt` and hex encoded. The salt is optional and can be an empty string.
// If `hash_variable` is false, the output is a
// user-friendly version of the effective DNS URI template. This value is used
// to inform the user of identifiers which are shared with the DoH server when
// sending a DNS resolution request.
// Only affiliated users can share device identifiers. If the user is not
// affiliated, the device identifier placeholder will be replaced by
// `kDeviceNotManaged`; e.g for `hash_variable`=true
// ${DEVICE_ASSET_ID} is replaced by hash(VALUE_NOT_AVAILABLE+salt).
std::string ReplaceVariables(std::string templates,
                             const std::string salt,
                             policy::DeviceAttributes* attributes,
                             bool hash_variable) {
  if (!user_manager::UserManager::IsInitialized()) {
    return std::string();
  }
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    return std::string();
  }

  std::string user_email = user->GetAccountId().GetUserEmail();
  std::string user_email_domain = EmailDomain(user_email);
  std::string user_email_name = EmailName(user_email);
  std::string original_templates = templates;
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kUserEmailPlaceholder,
      FormatVariable(user_email, salt, hash_variable));
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kUserEmailDomainPlaceholder,
      FormatVariable(user_email_domain, salt, hash_variable));
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kUserEmailNamePlaceholder,
      FormatVariable(user_email_name, salt, hash_variable));

  std::string device_directory_id = kDeviceNotManaged;
  std::string device_asset_id = kDeviceNotManaged;
  std::string device_serial_number = kDeviceNotManaged;
  std::string device_annotated_location = kDeviceNotManaged;

  if (user->IsAffiliated() && attributes) {
    device_directory_id = attributes->GetDirectoryApiID();
    device_asset_id = attributes->GetDeviceAssetID();
    device_serial_number = attributes->GetDeviceSerialNumber();
    device_annotated_location = attributes->GetDeviceAnnotatedLocation();
  } else {
    // Device identifiers are only replaced for affiliated users.
    LOG(WARNING)
        << "Skipping device variables replacement for unaffiliated user";
  }

  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kDeviceDirectoryIdPlaceholder,
      FormatVariable(device_directory_id, salt, hash_variable));
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kDeviceAssetIdPlaceholder,
      FormatVariable(device_asset_id, salt, hash_variable));
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kDeviceSerialNumberPlaceholder,
      FormatVariable(device_serial_number, salt, hash_variable));
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kDeviceAnnotatedLocationPlaceholder,
      FormatVariable(device_annotated_location, salt, hash_variable));

  // The device IP addresses are not hashed in the DNS URI template. In this
  // case, `hash_variable` is used to indicate if the IP addresses should be
  // replaced with a string that represents the network byte order (required by
  // the DNS server) or as a human-readable string used for privacy disclosure.
  base::ReplaceSubstringsAfterOffset(
      &templates, 0, kDeviceIpPlaceholder,
      GetIpReplacementValue(/*use_network_byte_order=*/hash_variable, *user));

  bool is_display_mode = !hash_variable;
  if (is_display_mode) {
    HighlightUnknownDisplayPlaceholders(templates, original_templates);
  } else {
    StripUnknownEffectivePlaceholders(templates);
  }

  return templates;
}

}  // namespace

namespace ash::dns_over_https {

TemplatesUriResolverImpl::TemplatesUriResolverImpl() {
  attributes_ = std::make_unique<policy::DeviceAttributesImpl>();
}

TemplatesUriResolverImpl::~TemplatesUriResolverImpl() = default;

void TemplatesUriResolverImpl::Update(PrefService* pref_service) {
  doh_with_identifiers_active_ = false;

  const std::string& mode = pref_service->GetString(prefs::kDnsOverHttpsMode);
  if (mode == SecureDnsConfig::kModeOff) {
    return;
  }

  effective_templates_ = pref_service->GetString(prefs::kDnsOverHttpsTemplates);
  // In ChromeOS only, the DnsOverHttpsTemplatesWithIdentifiers policy will
  // overwrite the DnsOverHttpsTemplates policy. For privacy reasons, the
  // replacement only happens if the is a salt specified which will be used to
  // hash the identifiers in the template URI.
  std::string templates_with_identifiers =
      pref_service->GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  std::string salt = pref_service->GetString(prefs::kDnsOverHttpsSalt);

  if (!salt.empty() &&
      (salt.size() < kMinSaltSize || salt.size() > kMaxSaltSize)) {
    // If the salt is set but the size is not within the specified limits, then
    // we ignore the config. This should have been checked upfront so no need to
    // report here.
    return;
  }

  std::string effective_templates =
      ReplaceVariables(templates_with_identifiers, salt, attributes_.get(),
                       /*hash_variable=*/true);
  std::string display_templates =
      ReplaceVariables(templates_with_identifiers, "", attributes_.get(),
                       /*hash_variable=*/false);
  if (effective_templates.empty() || display_templates.empty()) {
    return;
  }
  // We only use this if the variable substitution was successful for both
  // effective and display templates. Otherwise something is wrong and this
  // should have been reported earlier.
  effective_templates_ = effective_templates;
  display_templates_ = display_templates;
  doh_with_identifiers_active_ = true;
}

bool TemplatesUriResolverImpl::GetDohWithIdentifiersActive() {
  return doh_with_identifiers_active_;
}

std::string TemplatesUriResolverImpl::GetEffectiveTemplates() {
  return effective_templates_;
}

std::string TemplatesUriResolverImpl::GetDisplayTemplates() {
  return display_templates_;
}

void TemplatesUriResolverImpl::SetDeviceAttributesForTesting(
    std::unique_ptr<policy::FakeDeviceAttributes> attributes) {
  CHECK_IS_TEST();
  attributes_ = std::move(attributes);
}

// static
bool TemplatesUriResolverImpl::IsDeviceIpAddressIncludedInUriTemplate(
    std::string_view uri_templates) {
  return uri_templates.find(kDeviceIpPlaceholder) != std::string::npos;
}

}  // namespace ash::dns_over_https
