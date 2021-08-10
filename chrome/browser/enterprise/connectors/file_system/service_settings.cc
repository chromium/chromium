// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

const base::Feature kFileSystemConnectorEnabled{
    "FileSystemConnectorsEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

FileSystemServiceSettings::FileSystemServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_ =
        service_provider_config.GetServiceProvider(*service_provider_name);
  }
  if (!service_provider_)
    return;

  service_provider_name_ = *service_provider_name;

  // The domain will not be present if the admin has not set it.
  const std::string* domain = settings_value.FindStringKey(kKeyDomain);
  if (domain)
    email_domain_ = *domain;

  // Add the patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::URLMatcherConditionSet::ID id(0);
  const base::Value* enable = settings_value.FindListKey(kKeyEnable);
  if (enable && enable->is_list() && !enable->GetList().empty()) {
    filters_validated_ = true;
    for (const base::Value& value : enable->GetList()) {
      filters_validated_ &=
          AddUrlPatternSettings(value, /* enabled = */ true, &id);
    }
    LOG_IF(ERROR, !filters_validated_) << "Invalid filters: " << settings_value;
  } else {
    LOG(ERROR) << "Find no enable field in policy: " << settings_value;
    filters_validated_ = false;
    return;
  }

  const base::Value* disable = settings_value.FindListKey(kKeyDisable);
  if (disable && disable->is_list() && !disable->GetList().empty()) {
    for (const base::Value& value : disable->GetList()) {
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
  settings.home = GURL(service_provider_->fs_home_url());
  settings.authorization_endpoint =
      GURL(service_provider_->fs_authorization_endpoint());
  settings.token_endpoint = GURL(service_provider_->fs_token_endpoint());
  settings.enterprise_id = this->enterprise_id_;
  settings.email_domain = this->email_domain_;
  settings.client_id = service_provider_->fs_client_id();
  settings.client_secret = service_provider_->fs_client_secret();
  settings.scopes = service_provider_->fs_scopes();
  settings.max_direct_size = service_provider_->fs_max_direct_size();

  return settings;
}

absl::optional<FileSystemSettings> FileSystemServiceSettings::GetSettings(
    const GURL& url) const {
  if (!IsValid())
    return absl::nullopt;

  DCHECK(url.is_valid()) << "URL: " << url;

  absl::optional<FileSystemSettings> settings = GetGlobalSettings();
  if (settings.has_value()) {
    DCHECK(matcher_);
    std::set<std::string> mime_types;
    auto matches = matcher_->MatchURL(url);
    if (matches.empty())
      return absl::nullopt;
    mime_types = GetMimeTypes(matches);
    if (mime_types.empty())
      return absl::nullopt;

    settings->mime_types = std::move(mime_types);
  }

  return settings;
}

// static
absl::optional<FileSystemServiceSettings::URLPatternSettings>
FileSystemServiceSettings::GetPatternSettings(
    const PatternSettings& patterns,
    url_matcher::URLMatcherConditionSet::ID match) {
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
  return service_provider_ && filters_validated_ && !enterprise_id_.empty();
}

bool FileSystemServiceSettings::AddUrlsDisabledByServiceProvider(
    url_matcher::URLMatcherConditionSet::ID* id) {
  base::Value disable_value(base::Value::Type::DICTIONARY);

  std::vector<base::Value> urls;
  for (const std::string& url : service_provider_->fs_disable())
    urls.emplace_back(url);
  disable_value.SetKey(kKeyUrlList, base::Value(urls));

  std::vector<base::Value> mime_types;
  mime_types.emplace_back(kWildcardMimeType);
  disable_value.SetKey(kKeyMimeTypes, base::Value(mime_types));

  bool validated =
      AddUrlPatternSettings(disable_value, /* enabled = */ false, id);
  LOG_IF(ERROR, !validated)
      << "Invalid filters by service provider " << disable_value;
  return validated;
}

bool FileSystemServiceSettings::AddUrlPatternSettings(
    const base::Value& url_settings_value,
    bool enabled,
    url_matcher::URLMatcherConditionSet::ID* id) {
  DCHECK(id);
  DCHECK(service_provider_);
  if (enabled) {
    if (!disabled_patterns_settings_.empty()) {
      LOG(ERROR) << "disabled_patterns_settings_ must be empty when enabling: "
                 << url_settings_value;
      return false;
    }
  } else if (enabled_patterns_settings_.empty()) {
    LOG(ERROR) << "enabled_patterns_settings_ can't be empty when disabling: "
               << url_settings_value;
    return false;
  }

  URLPatternSettings setting;

  const base::Value* mime_types = url_settings_value.FindListKey(kKeyMimeTypes);
  if (!mime_types || !mime_types->is_list()) {
    return false;
  }
  for (const base::Value& mime_type : mime_types->GetList()) {
    if (mime_type.is_string()) {
      setting.mime_types.insert(mime_type.GetString());
    }
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value* url_list = url_settings_value.FindListKey(kKeyUrlList);
  if (!url_list || !url_list->is_list()) {
    return false;
  }
  const base::ListValue* url_list_value = nullptr;
  url_list->GetAsList(&url_list_value);
  DCHECK(url_list_value);
  policy::url_util::AddFilters(matcher_.get(), enabled, id, url_list_value);

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);

  return true;
}

std::set<std::string> FileSystemServiceSettings::GetMimeTypes(
    const std::set<url_matcher::URLMatcherConditionSet::ID>& matches) const {
  std::set<std::string> enable_mime_types;
  std::set<std::string> disable_mime_types;
  for (const url_matcher::URLMatcherConditionSet::ID match : matches) {
    // Enabled patterns need to be checked first, otherwise they always match
    // the first disabled pattern.
    bool enable = true;
    auto maybe_pattern_setting =
        GetPatternSettings(enabled_patterns_settings_, match);
    if (!maybe_pattern_setting.has_value()) {
      maybe_pattern_setting =
          GetPatternSettings(disabled_patterns_settings_, match);
      enable = false;
    }

    DCHECK(maybe_pattern_setting.has_value());
    auto mime_types = std::move(maybe_pattern_setting.value().mime_types);
    if (enable)
      enable_mime_types.insert(mime_types.begin(), mime_types.end());
    else
      disable_mime_types.insert(mime_types.begin(), mime_types.end());
  }

  // Disable takes precedence in case of conflicting logic.
  for (const std::string& mime_type_to_disable : disable_mime_types) {
    if (mime_type_to_disable == kWildcardMimeType) {
      LOG_IF(ERROR, disable_mime_types.size() > 1U) << "Already has wildcard";
      enable_mime_types.clear();
      break;
    }
    enable_mime_types.erase(mime_type_to_disable);
  }

  return enable_mime_types;
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
