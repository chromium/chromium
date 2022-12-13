// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/url_formatter/url_fixer.h"
#include "crypto/sha2.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

constexpr const char kSettingsResetPromptFeatureName[] = "SettingsResetPrompt";

bool IsPromptEnabled() {
  return base::FeatureList::IsEnabled(kSettingsResetPrompt);
}

}  // namespace.

BASE_FEATURE(kSettingsResetPrompt,
             kSettingsResetPromptFeatureName,
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
std::unique_ptr<SettingsResetPromptConfig> SettingsResetPromptConfig::Create() {
  if (!IsPromptEnabled())
    return nullptr;

  auto prompt_config = base::WrapUnique(new SettingsResetPromptConfig());
  if (!prompt_config->Init())
    return nullptr;

  return prompt_config;
}

SettingsResetPromptConfig::SettingsResetPromptConfig() {}

SettingsResetPromptConfig::~SettingsResetPromptConfig() {}

int SettingsResetPromptConfig::UrlToResetDomainId(const GURL& url) const {
  DCHECK(IsPromptEnabled());

  // Do a best-effort to fix the URL before testing if it is valid.
  GURL fixed_url =
      url_formatter::FixupURL(url.possibly_invalid_spec(), std::string());
  if (!fixed_url.is_valid())
    return -1;

  // Get the length of the top level domain or registry of the URL. Used
  // to guard against trying to match the (effective) TLDs themselves.
  size_t registry_length = net::registry_controlled_domains::GetRegistryLength(
      fixed_url, net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // Do not proceed, if |fixed_url| does not have a host or consists entirely of
  // a registry or top domain.
  if (registry_length == 0 || registry_length == std::string::npos)
    return -1;

  // The hashes in the prompt config are generally TLD+1 and identify
  // only the topmost levels of URLs that we wish to prompt for. Try to
  // match each sensible suffix of the URL host with the hashes in the
  // prompt config. For example, if the host is
  // "www.sub.domain.com", try hashes for:
  // "www.sub.domain.com"
  // "sub.domain.com"
  // "domain.com"
  // We Do not check top level or registry domains to guard against bad
  // configuration data.
  SHA256Hash hash(crypto::kSHA256Length, '\0');
  base::StringPiece host = fixed_url.host_piece();
  while (host.size() > registry_length) {
    crypto::SHA256HashString(host, hash.data(), crypto::kSHA256Length);
    auto iter = domain_hashes_.find(hash);
    if (iter != domain_hashes_.end())
      return iter->second;

    size_t next_start_pos = host.find('.');
    next_start_pos = next_start_pos == base::StringPiece::npos
                         ? base::StringPiece::npos
                         : next_start_pos + 1;
    host = host.substr(next_start_pos);
  }

  return -1;
}

base::TimeDelta SettingsResetPromptConfig::delay_before_prompt() const {
  return delay_before_prompt_;
}

int SettingsResetPromptConfig::prompt_wave() const {
  return prompt_wave_;
}

base::TimeDelta SettingsResetPromptConfig::time_between_prompts() const {
  return time_between_prompts_;
}

// Implements the hash function for SHA256Hash objects. Simply uses the
// first bytes of the SHA256 hash as its own hash.
size_t SettingsResetPromptConfig::SHA256HashHasher::operator()(
    const SHA256Hash& key) const {
  DCHECK_EQ(crypto::kSHA256Length, key.size());
  // This is safe because |key| contains 32 bytes while a size_t is
  // either 4 or 8 bytes.
  return *reinterpret_cast<const size_t*>(key.data());
}

// These values are written to logs. New enum values can be added, but
// existing enums must never be renumbered or deleted and reused. If you
// do add values, also update the corresponding enum definition in the
// histograms.xml file.
enum SettingsResetPromptConfig::ConfigError : int {
  CONFIG_ERROR_OK = 1,
  CONFIG_ERROR_MISSING_DOMAIN_HASHES_PARAM = 2,
  CONFIG_ERROR_BAD_DOMAIN_HASHES_PARAM = 3,
  CONFIG_ERROR_BAD_DOMAIN_HASH = 4,
  CONFIG_ERROR_BAD_DOMAIN_ID = 5,
  CONFIG_ERROR_DUPLICATE_DOMAIN_HASH = 6,
  CONFIG_ERROR_BAD_DELAY_BEFORE_PROMPT_SECONDS_PARAM = 7,
  CONFIG_ERROR_BAD_PROMPT_WAVE_PARAM = 8,
  CONFIG_ERROR_BAD_TIME_BETWEEN_PROMPTS_SECONDS_PARAM = 9,
  CONFIG_ERROR_MAX
};

bool SettingsResetPromptConfig::Init() {
  if (!IsPromptEnabled())
    return false;

  // Parse the domain_hashes feature parameter.
  std::string domain_hashes_json = base::GetFieldTrialParamValueByFeature(
      kSettingsResetPrompt, "domain_hashes");
  ConfigError error = ParseDomainHashes(domain_hashes_json);
  if (error != CONFIG_ERROR_OK) {
    UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ConfigError", error,
                              CONFIG_ERROR_MAX);
    return false;
  }

  // Get the delay_before_prompt feature parameter.
  int delay_before_prompt_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kSettingsResetPrompt, "delay_before_prompt_seconds", -1);
  if (delay_before_prompt_seconds < 0) {
    UMA_HISTOGRAM_ENUMERATION(
        "SettingsResetPrompt.ConfigError",
        CONFIG_ERROR_BAD_DELAY_BEFORE_PROMPT_SECONDS_PARAM, CONFIG_ERROR_MAX);
    return false;
  }
  delay_before_prompt_ = base::Seconds(delay_before_prompt_seconds);

  // Get the prompt_wave feature paramter.
  prompt_wave_ = base::GetFieldTrialParamByFeatureAsInt(kSettingsResetPrompt,
                                                        "prompt_wave", 0);
  if (prompt_wave_ <= 0) {
    UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ConfigError",
                              CONFIG_ERROR_BAD_PROMPT_WAVE_PARAM,
                              CONFIG_ERROR_MAX);
    return false;
  }

  // Get the time_between_prompts_seconds feature parameter.
  int time_between_prompts_seconds = base::GetFieldTrialParamByFeatureAsInt(
      kSettingsResetPrompt, "time_between_prompts_seconds", -1);
  if (time_between_prompts_seconds < 0) {
    UMA_HISTOGRAM_ENUMERATION(
        "SettingsResetPrompt.ConfigError",
        CONFIG_ERROR_BAD_TIME_BETWEEN_PROMPTS_SECONDS_PARAM, CONFIG_ERROR_MAX);
    return false;
  }
  time_between_prompts_ = base::Seconds(time_between_prompts_seconds);

  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ConfigError", CONFIG_ERROR_OK,
                            CONFIG_ERROR_MAX);
  return true;
}

SettingsResetPromptConfig::ConfigError
SettingsResetPromptConfig::ParseDomainHashes(
    const std::string& domain_hashes_json) {
  if (domain_hashes_json.empty())
    return CONFIG_ERROR_MISSING_DOMAIN_HASHES_PARAM;

  // Is the input parseable JSON?
  absl::optional<base::Value> domains_dict =
      base::JSONReader::Read(domain_hashes_json);
  if (!domains_dict || !domains_dict->is_dict() ||
      domains_dict->GetDict().empty()) {
    return CONFIG_ERROR_BAD_DOMAIN_HASHES_PARAM;
  }

  // The input JSON should be a hash object with hex-encoded 32-byte
  // hashes as keys and integer IDs as values. For example,
  //
  // {"2714..D7": "1", "2821..CB": "2", ...}
  //
  // Each key in the hash should be a 64-byte long string and each
  // integer ID should fit in an int.
  domain_hashes_.clear();
  for (const auto item : domains_dict->GetDict()) {
    const std::string& hash_string = item.first;
    if (hash_string.size() != crypto::kSHA256Length * 2)
      return CONFIG_ERROR_BAD_DOMAIN_HASH;

    // Convert hex-encoded hash string to its numeric value as bytes.
    SHA256Hash hash;
    hash.reserve(crypto::kSHA256Length);
    if (!base::HexStringToBytes(hash_string, &hash))
      return CONFIG_ERROR_BAD_DOMAIN_HASH;

    // Convert the ID string to an integer.
    const std::string* domain_id_string = item.second.GetIfString();
    int domain_id = -1;
    if (!domain_id_string ||
        !base::StringToInt(*domain_id_string, &domain_id) || domain_id < 0) {
      return CONFIG_ERROR_BAD_DOMAIN_ID;
    }

    if (!domain_hashes_.insert(std::make_pair(std::move(hash), domain_id))
             .second) {
      return CONFIG_ERROR_DUPLICATE_DOMAIN_HASH;
    }
  }

  return CONFIG_ERROR_OK;
}

}  // namespace safe_browsing.
