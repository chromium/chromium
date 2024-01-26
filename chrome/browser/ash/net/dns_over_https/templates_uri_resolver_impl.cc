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
// Used as a replacement value for device identifiers when the user is
// unaffiliated.
constexpr char kDeviceNotManaged[] = "VALUE_NOT_AVAILABLE";

constexpr char kFixedSaltForExperiment[] = "salt for experiment";

// Part before "@" of the given |email| address.
// "some_email@domain.com" => "some_email"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailName(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos)
    return std::string();
  return email.substr(0, at_sign_pos);
}

// Part after "@" of an email address.
// "some_email@domain.com" => "domain.com"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailDomain(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos)
    return std::string();
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
  std::string hash = crypto::SHA256HashString(salt + input);
  return base::HexEncode(hash.c_str(), hash.size());
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
        << "Skiping device variables replacement for unaffiliated user";
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

  return templates;
}

}  // namespace

namespace ash::dns_over_https {

TemplatesUriResolverImpl::TemplatesUriResolverImpl() {
  attributes_ = std::make_unique<policy::DeviceAttributesImpl>();
}

TemplatesUriResolverImpl::~TemplatesUriResolverImpl() = default;

void TemplatesUriResolverImpl::UpdateFromPrefs(PrefService* pref_service) {
  doh_with_identifiers_active_ = false;

  const std::string& mode = pref_service->GetString(prefs::kDnsOverHttpsMode);
  if (mode == SecureDnsConfig::kModeOff)
    return;

  effective_templates_ = pref_service->GetString(prefs::kDnsOverHttpsTemplates);
  if (!features::IsDnsOverHttpsWithIdentifiersEnabled())
    return;
  // In ChromeOS only, the DnsOverHttpsTemplatesWithIdentifiers policy will
  // overwrite the DnsOverHttpsTemplates policy. For privacy reasons, the
  // replacement only happens if the is a salt specified which will be used to
  // hash the identifiers in the template URI.
  std::string templates_with_identifiers =
      pref_service->GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  // Until the DnsOverHttpsTemplatesWithIdentifiers policy is added to DPanel,
  // the templates with identifiers can be specified via the old policy,
  // `kDnsOverHttpsTemplates` to enable early costumer testing. This testing
  // mode is controlled by the flag
  // features::kDnsOverHttpsWithIdentifiersReuseOldPolicy.
  // TODO(acostinas, srad, b/233845305) Remove when policy is added to DPanel.
  if (templates_with_identifiers.empty() &&
      features::IsDnsOverHttpsWithIdentifiersReuseOldPolicyEnabled()) {
    templates_with_identifiers = effective_templates_;
  }
  std::string salt = pref_service->GetString(prefs::kDnsOverHttpsSalt);
  // TODO(acostinas, srad, b/233845305) Remove when policy is added to DPanel.
  if (salt.empty() &&
      features::IsDnsOverHttpsWithIdentifiersReuseOldPolicyEnabled()) {
    salt = kFixedSaltForExperiment;
  }
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
  if (effective_templates.empty() || display_templates.empty())
    return;
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

}  // namespace ash::dns_over_https
