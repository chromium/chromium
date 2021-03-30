// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "base/json/json_reader.h"

#if defined(USE_OFFICIAL_ENTERPRISE_CONNECTORS_API_KEYS)
#include "google_apis/internal/enterprise_connectors_api_keys.h"
#endif

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

#if !defined(CLIENT_ID_CONNECTOR_PARTNER_BOX)
#define CLIENT_ID_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

#if !defined(CLIENT_SECRET_CONNECTOR_PARTNER_BOX)
#define CLIENT_SECRET_CONNECTOR_PARTNER_BOX DUMMY_API_TOKEN
#endif

namespace enterprise_connectors {

namespace {

// Keys used to read service provider config values.
constexpr char kKeyVersion[] = "version";
constexpr char kKeyServiceProviders[] = "service_providers";
constexpr char kKeyName[] = "name";
constexpr char kKeyAnalysis[] = "analysis";
constexpr char kKeyFileSystem[] = "file_system";
constexpr char kKeyReporting[] = "reporting";
constexpr char kKeyUrl[] = "url";
constexpr char kKeySupportedTags[] = "supported_tags";
constexpr char kKeyMimeTypes[] = "mime_types";
constexpr char kKeyMaxFileSize[] = "max_file_size";

// Specific key names of file system connectors.
constexpr char kKeyFsHome[] = "home";
constexpr char kKeyFsAuthorizationEndpoint[] = "authorization_endpoint";
constexpr char kKeyFsTokenEndpoint[] = "token_endpoint";
constexpr char kKeyFsMaxDirectZize[] = "max_direct_size";
constexpr char kKeyFsScopes[] = "home";
constexpr char kKeyFsDisable[] = "disable";

// There is currently only 1 version of this config, so we can just treat it as
// any other value in the JSON with its own key. Once that is no longer the
// case extra code should be added to decide on the correct version to read.
constexpr char kKeyV1[] = "1";

}  // namespace

ServiceProviderConfig::ServiceProviderConfig(const std::string& config) {
  auto config_value = base::JSONReader::Read(config);
  if (!config_value.has_value() || !config_value.value().is_dict())
    return;

  const base::Value* service_providers =
      config_value.value().FindListKey(kKeyServiceProviders);
  if (!service_providers)
    return;

  for (const base::Value& service_provider_value :
       service_providers->GetList()) {
    const std::string* name = service_provider_value.FindStringKey(kKeyName);
    if (name)
      service_providers_.emplace(*name, service_provider_value);
  }
}

std::vector<std::string> ServiceProviderConfig::GetServiceProviderNames()
    const {
  std::vector<std::string> names;
  names.reserve(service_providers_.size());
  for (const auto& name_and_config : service_providers_)
    names.push_back(name_and_config.first);
  return names;
}

const ServiceProviderConfig::ServiceProvider*
ServiceProviderConfig::GetServiceProvider(
    const std::string& service_provider) const {
  if (service_providers_.count(service_provider) == 0)
    return nullptr;
  return &service_providers_.at(service_provider);
}

ServiceProviderConfig::ServiceProviderConfig(ServiceProviderConfig&&) = default;
ServiceProviderConfig::~ServiceProviderConfig() = default;

ServiceProviderConfig::ServiceProvider::ServiceProvider(
    const base::Value& config) {
  if (!config.is_dict())
    return;

  const std::string* name = config.FindStringKey(kKeyName);
  if (!name)
    return;

  const base::Value* versions = config.FindDictKey(kKeyVersion);
  if (!versions || !versions->is_dict())
    return;

  const base::Value* version_1 = versions->FindDictKey(kKeyV1);
  if (!version_1 || !version_1->is_dict())
    return;

  const base::Value* analysis = version_1->FindDictKey(kKeyAnalysis);
  if (analysis && analysis->is_dict()) {
    const std::string* analysis_url = analysis->FindStringKey(kKeyUrl);
    if (analysis_url)
      analysis_url_ = *analysis_url;

    const base::Value* supported_tags =
        analysis->FindListKey(kKeySupportedTags);
    if (supported_tags) {
      for (const base::Value& tag : supported_tags->GetList()) {
        if (!tag.is_dict())
          continue;

        const std::string* name = tag.FindStringKey(kKeyName);
        if (name)
          analysis_tags_.emplace(*name, tag);
      }
    }
  }

  const base::Value* reporting = version_1->FindDictKey(kKeyReporting);
  if (reporting) {
    const std::string* reporting_url = reporting->FindStringKey(kKeyUrl);
    if (reporting_url)
      reporting_url_ = *reporting_url;
  }

  const base::Value* file_system = version_1->FindDictKey(kKeyFileSystem);
  if (file_system) {
    const std::string* home_url = file_system->FindStringKey(kKeyFsHome);
    if (home_url)
      fs_home_url_ = *home_url;

    const std::string* auth_endpoint_url =
        file_system->FindStringKey(kKeyFsAuthorizationEndpoint);
    if (auth_endpoint_url)
      fs_authorization_endpoint_ = *auth_endpoint_url;

    const std::string* token_endpoint =
        file_system->FindStringKey(kKeyFsTokenEndpoint);
    if (token_endpoint)
      fs_token_endpoint_ = *token_endpoint;

    auto max_direct_size = file_system->FindIntKey(kKeyFsMaxDirectZize);
    if (max_direct_size)
      fs_max_direct_size_ = max_direct_size.value();

    // Client ID and secret, per partner, are not stored in the
    // kServiceProviderConfig string to keep them out of the open source
    // repo.
    if (*name == "box") {
      fs_client_id_ = CLIENT_ID_CONNECTOR_PARTNER_BOX;
      fs_client_secret_ = CLIENT_SECRET_CONNECTOR_PARTNER_BOX;
    } else {
      fs_client_id_ = DUMMY_API_TOKEN;
      fs_client_secret_ = DUMMY_API_TOKEN;
    }

    const base::Value* scopes = file_system->FindListKey(kKeyFsScopes);
    if (scopes) {
      for (const base::Value& scope : scopes->GetList()) {
        if (!scope.is_string())
          continue;

        fs_scopes_.push_back(scope.GetString());
      }
    }

    const base::Value* disbles = file_system->FindListKey(kKeyFsDisable);
    if (disbles) {
      for (const base::Value& disable : disbles->GetList()) {
        if (!disable.is_string())
          continue;

        fs_disable_.push_back(disable.GetString());
      }
    }
  }
}

ServiceProviderConfig::ServiceProvider::ServiceProvider(ServiceProvider&&) =
    default;
ServiceProviderConfig::ServiceProvider::~ServiceProvider() = default;

ServiceProviderConfig::ServiceProvider::Tag::Tag(const base::Value& tag_value) {
  if (!tag_value.is_dict())
    return;

  const base::Value* mime_types = tag_value.FindListKey(kKeyMimeTypes);
  if (mime_types) {
    for (const base::Value& mime_type : mime_types->GetList()) {
      if (!mime_type.is_string())
        continue;

      mime_types_.push_back(mime_type.GetString());
    }
  }

  max_file_size_ = tag_value.FindIntKey(kKeyMaxFileSize).value_or(-1);
}

ServiceProviderConfig::ServiceProvider::Tag::Tag(Tag&&) = default;
ServiceProviderConfig::ServiceProvider::Tag::~Tag() = default;

}  // namespace enterprise_connectors
