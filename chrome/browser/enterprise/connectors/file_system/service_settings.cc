// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

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

  const std::string* enterprise_id =
      settings_value.FindStringKey(kKeyEnterpriseId);
  if (enterprise_id) {
    enterprise_id_ = *enterprise_id;
  } else {
    return;
  }

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
    for (const base::Value& value : enable->GetList())
      AddUrlPatternSettings(value, true, &id);
  } else {
    return;
  }

  const base::Value* disable = settings_value.FindListKey(kKeyDisable);
  if (disable && disable->is_list()) {
    for (const base::Value& value : disable->GetList())
      AddUrlPatternSettings(value, false, &id);
  }

  // Add all the URLs automatically disabled by the service provider.
  base::Value disable_value(base::Value::Type::DICTIONARY);

  std::vector<base::Value> urls;
  for (const std::string& url : service_provider_->fs_disable())
    urls.emplace_back(url);
  disable_value.SetKey(kKeyUrlList, base::Value(urls));

  std::vector<base::Value> mime_types;
  mime_types.emplace_back(kWildcardMimeType);
  disable_value.SetKey(kKeyMimeTypes, base::Value(mime_types));

  AddUrlPatternSettings(disable_value, false, &id);
}

FileSystemServiceSettings::FileSystemServiceSettings(
    FileSystemServiceSettings&&) = default;
FileSystemServiceSettings::~FileSystemServiceSettings() = default;

base::Optional<FileSystemSettings> FileSystemServiceSettings::GetSettings(
    const GURL& url) const {
  if (!IsValid())
    return base::nullopt;

  DCHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty())
    return base::nullopt;

  auto mime_types = GetMimeTypes(matches);
  if (mime_types.empty())
    return base::nullopt;

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
  settings.mime_types = std::move(mime_types);

  return settings;
}

// static
base::Optional<FileSystemServiceSettings::URLPatternSettings>
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

  return base::nullopt;
}

bool FileSystemServiceSettings::IsValid() const {
  // The settings are valid only if a provider was given.
  return service_provider_ && !enterprise_id_.empty();
}

void FileSystemServiceSettings::AddUrlPatternSettings(
    const base::Value& url_settings_value,
    bool enabled,
    url_matcher::URLMatcherConditionSet::ID* id) {
  DCHECK(id);
  DCHECK(service_provider_);
  if (enabled)
    DCHECK(disabled_patterns_settings_.empty());
  else
    DCHECK(!enabled_patterns_settings_.empty());

  URLPatternSettings setting;

  const base::Value* mime_types = url_settings_value.FindListKey(kKeyMimeTypes);
  if (mime_types && mime_types->is_list()) {
    for (const base::Value& mime_type : mime_types->GetList()) {
      if (mime_type.is_string()) {
        setting.mime_types.insert(mime_type.GetString());
      }
    }
  } else {
    return;
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value* url_list = url_settings_value.FindListKey(kKeyUrlList);
  if (url_list && url_list->is_list()) {
    const base::ListValue* url_list_value = nullptr;
    url_list->GetAsList(&url_list_value);
    DCHECK(url_list_value);
    policy::url_util::AddFilters(matcher_.get(), enabled, id, url_list_value);
  } else {
    return;
  }

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);
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

  for (const std::string& mime_type_to_disable : disable_mime_types) {
    if (mime_type_to_disable == kWildcardMimeType) {
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
