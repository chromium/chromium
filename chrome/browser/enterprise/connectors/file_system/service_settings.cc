// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/url_matcher/url_util.h"

namespace enterprise_connectors {

const base::Feature kFileSystemConnectorEnabled{
    "FileSystemConnectorsEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

FileSystemServiceSettings::FileSystemServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict()) {
    DLOG(ERROR) << "Settings passed in is not dict: " << settings_value;
    return;
  }

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider_name &&
      service_provider_config.count(*service_provider_name)) {
    file_system_config_ =
        service_provider_config.at(*service_provider_name).file_system;
  }
  if (!file_system_config_) {
    DLOG(ERROR) << "No file system config for corresponding service provider";
    return;
  }

  service_provider_name_ = *service_provider_name;

  // The domain will not be present if the admin has not set it.
  const std::string* domain = settings_value.FindStringKey(kKeyDomain);
  if (domain)
    email_domain_ = *domain;

  // Load and validate all the URL and MIME type filters.
  filters_validated_ = true;
  // Add the patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  URLMatchingID id(0);
  const base::Value* enable = settings_value.FindListKey(kKeyEnable);
  if (enable && enable->is_list() && !enable->GetListDeprecated().empty()) {
    filters_validated_ = true;
    for (const base::Value& value : enable->GetListDeprecated()) {
      filters_validated_ &=
          AddUrlPatternSettings(value, /* enabled = */ true, &id);
    }
    LOG_IF(ERROR, !filters_validated_) << "Invalid filters: " << settings_value;
  } else {
    DLOG(ERROR) << "Find no enable field in policy: " << settings_value;
    filters_validated_ = false;
    return;
  }

  const base::Value* disable = settings_value.FindListKey(kKeyDisable);
  if (disable && disable->is_list() && !disable->GetListDeprecated().empty()) {
    for (const base::Value& value : disable->GetListDeprecated()) {
      filters_validated_ &=
          AddUrlPatternSettings(value, /* enabled = */ false, &id);
    }
    LOG_IF(ERROR, !filters_validated_) << "Invalid filters: " << settings_value;
  }

  // Add all the URLs automatically disabled by the service provider.
  filters_validated_ &= AddUrlsDisabledByServiceProvider(&id);
  LOG_IF(ERROR, !filters_validated_) << "Filters could NOT be validated";

  // Extracct enterprise_id last so that empty enterprise_id does not prevent
  // the filters from being validated.
  const std::string* enterprise_id =
      settings_value.FindStringKey(kKeyEnterpriseId);
  if (enterprise_id) {
    enterprise_id_ = *enterprise_id;
  }
  DLOG_IF(ERROR, !enterprise_id) << "No enteprise id found! " << settings_value;
}

FileSystemServiceSettings::FileSystemServiceSettings(
    FileSystemServiceSettings&&) = default;
FileSystemServiceSettings::~FileSystemServiceSettings() = default;

absl::optional<FileSystemSettings>
FileSystemServiceSettings::GetGlobalSettings() const {
  if (!IsValid())
    return absl::nullopt;

  FileSystemSettings settings;
  settings.service_provider = service_provider_name_;
  settings.home = GURL(file_system_config_->home);
  settings.authorization_endpoint =
      GURL(file_system_config_->authorization_endpoint);
  settings.token_endpoint = GURL(file_system_config_->token_endpoint);
  settings.enterprise_id = this->enterprise_id_;
  settings.email_domain = this->email_domain_;
  settings.client_id = file_system_config_->client_id;
  settings.client_secret = file_system_config_->client_secret;
  settings.scopes = std::vector<std::string>(
      file_system_config_->scopes.begin(), file_system_config_->scopes.end());
  settings.max_direct_size = file_system_config_->max_direct_size;

  return settings;
}

absl::optional<FileSystemSettings> FileSystemServiceSettings::GetSettings(
    const GURL& url) const {
  if (!IsValid() || !url.is_valid())
    return absl::nullopt;

  absl::optional<FileSystemSettings> settings = GetGlobalSettings();

  if (!settings.has_value())
    return absl::nullopt;

  DCHECK(url_matcher_);
  auto matches = url_matcher_->MatchURL(url);
  if (matches.empty())
    return absl::nullopt;

  MimeTypesFilter mime_filter = GetMimeTypesFilterFromUrlMatches(matches);
  const std::set<std::string>& mime_types = mime_filter.first;
  if (mime_types.empty())
    return absl::nullopt;

  settings->enable_with_mime_types = mime_filter.second;
  settings->mime_types = std::move(mime_types);
  return settings;
}

// static
absl::optional<FileSystemServiceSettings::URLPatternSettings>
FileSystemServiceSettings::GetPatternSettings(const PatternSettings& patterns,
                                              URLMatchingID match) {
  // If the pattern exists directly in the map, return its settings.
  if (patterns.count(match) == 1)
    return patterns.at(match);

  // If the pattern doesn't exist in the map, it might mean that it wasn't the
  // only pattern to correspond to its settings and that the ID added to
  // the map was the one of the last pattern corresponding to those settings.
  // This means the next match ID greater than |match| has the correct settings
  // if it exists.
  auto next = patterns.upper_bound(match);
  if (next != patterns.end())
    return next->second;

  return absl::nullopt;
}

bool FileSystemServiceSettings::IsValid() const {
  // The settings are valid only if a provider was given.
  return file_system_config_ && filters_validated_ && !enterprise_id_.empty();
}

bool FileSystemServiceSettings::AddUrlsDisabledByServiceProvider(
    URLMatchingID* id) {
  base::Value disable_value(base::Value::Type::DICTIONARY);

  base::Value::List urls;
  for (const std::string& url : file_system_config_->disable)
    urls.Append(url);
  disable_value.SetKey(kKeyUrlList, base::Value(std::move(urls)));

  base::Value::List mime_types;
  mime_types.Append(kWildcardMimeType);
  disable_value.SetKey(kKeyMimeTypes, base::Value(std::move(mime_types)));

  bool validated =
      AddUrlPatternSettings(disable_value, /* enabled = */ false, id);
  LOG_IF(ERROR, !validated)
      << "Invalid filters by service provider " << disable_value;
  return validated;
}

bool FileSystemServiceSettings::AddUrlPatternSettings(
    const base::Value& url_settings_value,
    bool enabled,
    URLMatchingID* id) {
  DCHECK(id);
  DCHECK(file_system_config_);
  if (enabled) {
    if (!disabled_patterns_settings_.empty()) {
      DLOG(ERROR) << "disabled_patterns_settings_ must be empty when enabling: "
                  << url_settings_value;
      return false;
    }
  } else if (enabled_patterns_settings_.empty()) {
    DLOG(ERROR) << "enabled_patterns_settings_ can't be empty when disabling: "
                << url_settings_value;
    return false;
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value* url_list_value =
      url_settings_value.FindListKey(kKeyUrlList);
  if (!url_list_value) {
    DLOG(ERROR) << "Can't find " << kKeyUrlList << url_settings_value;
    return false;
  }

  const base::Value::List& url_list = url_list_value->GetList();

  for (const base::Value& url : url_list)
    CHECK(url.is_string());

  // This pre-increments the id by size of url_list_value.
  URLMatchingID pre_id = *id;
  url_matcher::util::AddFilters(url_matcher_.get(), enabled, id, url_list);

  const base::Value* mime_types = url_settings_value.FindListKey(kKeyMimeTypes);
  if (!mime_types)
    return false;

  URLPatternSettings setting;
  bool has_wildcard = false;
  for (const base::Value& mime_type : mime_types->GetListDeprecated()) {
    if (mime_type.is_string()) {
      const std::string& m = mime_type.GetString();
      setting.mime_types.insert(m);
      has_wildcard &= (m == kWildcardMimeType);
    }
  }

  for (URLMatchingID curr_id = pre_id + 1; curr_id <= *id; ++curr_id) {
    if (enabled) {
      enabled_patterns_settings_[*id] = setting;
    } else {  // disabled
      if (has_wildcard) {
        // Disable has precedence.
        enabled_patterns_settings_.erase(*id);
      }
      disabled_patterns_settings_[*id] = setting;
    }
  }

  return true;
}

FileSystemServiceSettings::MimeTypesFilter
FileSystemServiceSettings::GetMimeTypesFilterFromUrlMatches(
    const std::set<URLMatchingID>& matches) const {
  std::set<std::string> enable_mime_types;
  std::set<std::string> disable_mime_types;
  bool wildcard_enable_mime = false;

  for (const URLMatchingID match : matches) {
    // Enabled patterns need to be checked first, otherwise they always match
    // the first disabled pattern.
    absl::optional<FileSystemServiceSettings::URLPatternSettings>
        maybe_pattern_setting =
            GetPatternSettings(enabled_patterns_settings_, match);

    bool this_match_has_wildcard_enable = false;
    if (maybe_pattern_setting.has_value()) {
      const auto& mime_types = maybe_pattern_setting.value().mime_types;
      this_match_has_wildcard_enable =
          (std::find(mime_types.begin(), mime_types.end(), kWildcardMimeType) !=
           mime_types.end());
      enable_mime_types.insert(mime_types.begin(), mime_types.end());
    }

    if (!maybe_pattern_setting.has_value() ||
        (this_match_has_wildcard_enable &&
         disabled_patterns_settings_.count(match))) {
      maybe_pattern_setting =
          GetPatternSettings(disabled_patterns_settings_, match);

      if (maybe_pattern_setting.has_value()) {
        const auto& mime_types = maybe_pattern_setting.value().mime_types;
        disable_mime_types.insert(mime_types.begin(), mime_types.end());
      }
    }
    DCHECK(maybe_pattern_setting.has_value());
  }

  if (enable_mime_types.size() == 1) {
    const auto& enabled_for = enabled_patterns_settings_.cbegin()->second;
    wildcard_enable_mime =
        (*enabled_for.mime_types.cbegin() == kWildcardMimeType);
  }

  if (wildcard_enable_mime && disable_mime_types.size() > 0) {
    return std::make_pair(std::move(disable_mime_types), false);
  }

  // Disable takes precedence in case of conflicting logic.
  for (const std::string& mime_type_to_disable : disable_mime_types) {
    if (mime_type_to_disable == kWildcardMimeType) {
      DLOG_IF(ERROR, disable_mime_types.size() > 1U) << "Already has wildcard";
      enable_mime_types.clear();
      break;
    }
    enable_mime_types.erase(mime_type_to_disable);
  }
  return std::make_pair(std::move(enable_mime_types), true);
}

FileSystemServiceSettings::URLPatternSettings::URLPatternSettings() = default;
FileSystemServiceSettings::URLPatternSettings::URLPatternSettings(
    const FileSystemServiceSettings::URLPatternSettings&) = default;
FileSystemServiceSettings::URLPatternSettings::URLPatternSettings(
    FileSystemServiceSettings::URLPatternSettings&&) = default;
FileSystemServiceSettings::URLPatternSettings&
FileSystemServiceSettings::URLPatternSettings::operator=(
    const FileSystemServiceSettings::URLPatternSettings&) = default;
FileSystemServiceSettings::URLPatternSettings&
FileSystemServiceSettings::URLPatternSettings::operator=(
    FileSystemServiceSettings::URLPatternSettings&&) = default;
FileSystemServiceSettings::URLPatternSettings::~URLPatternSettings() = default;

}  // namespace enterprise_connectors
