// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"
#endif

namespace enterprise_connectors {

AnalysisServiceSettings::AnalysisServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config)
    : AnalysisServiceSettingsBase(settings_value, service_provider_config) {
  if (!analysis_config_) {
    // Parsing in the base class failed
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  const auto& settings_dict = settings_value.GetDict();

  // Add the source/destination patterns to the settings, which configures
  // settings.matcher and settings.*_pattern_settings. No enable patterns
  // implies the settings are invalid.
  const auto* enabled_pattern_settings_list =
      settings_dict.FindList(kKeyEnable);
  if (!enabled_pattern_settings_list ||
      enabled_pattern_settings_list->empty()) {
    return;
  }

  ParseSourceDestinationPatternSettings(enabled_pattern_settings_list, true);
  ParseSourceDestinationPatternSettings(settings_dict.FindList(kKeyDisable),
                                        false);
#endif

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  ParseVerificationSignatures(settings_value.GetDict());
#endif
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
void AnalysisServiceSettings::ParseVerificationSignatures(
    const base::Value::Dict& settings_dict) {
#if BUILDFLAG(IS_WIN)
  const char* verification_key = kKeyWindowsVerification;
#elif BUILDFLAG(IS_MAC)
  const char* verification_key = kKeyMacVerification;
#elif BUILDFLAG(IS_LINUX)
  const char* verification_key = kKeyLinuxVerification;
#endif

  const base::Value::List* signatures =
      settings_dict.FindListByDottedPath(verification_key);
  if (!signatures) {
    return;
  }

  for (auto& v : *signatures) {
    if (v.is_string()) {
      verification_signatures_.push_back(v.GetString());
    }
  }
}
#endif

std::optional<AnalysisSettings> AnalysisServiceSettings::GetAnalysisSettings(
    const GURL& url,
    DataRegion data_region) const {
  auto settings =
      AnalysisServiceSettingsBase::GetAnalysisSettings(url, data_region);
  // If this is a cloud analysis (in which case the base class already
  // initialized the cloud-specific settings), return the settings as is.
  if (!settings.has_value() || is_cloud_analysis()) {
    return settings;
  }

  settings->cloud_or_local_settings =
      CloudOrLocalAnalysisSettings(GetLocalAnalysisSettings());

  return settings;
}

LocalAnalysisSettings AnalysisServiceSettings::GetLocalAnalysisSettings()
    const {
  CHECK(is_local_analysis());

  LocalAnalysisSettings local_settings;
  local_settings.local_path = analysis_config_->local_path;
  local_settings.user_specific = analysis_config_->user_specific;
  local_settings.subject_names = analysis_config_->subject_names;
  // We assume all support_tags structs have the same max file size.
  local_settings.max_file_size =
      analysis_config_->supported_tags[0].max_file_size;
  local_settings.verification_signatures = verification_signatures_;

  return local_settings;
}

#if BUILDFLAG(IS_CHROMEOS)
std::optional<AnalysisSettings> AnalysisServiceSettings::GetAnalysisSettings(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    DataRegion data_region) const {
  if (!IsValid()) {
    return std::nullopt;
  }

  CHECK(source_destination_matcher_);

  auto matches =
      source_destination_matcher_->Match(context, source_url, destination_url);
  if (matches.empty()) {
    return std::nullopt;
  }

  auto settings =
      AnalysisServiceSettingsBase::GetCommonAnalysisSettings(matches);
  if (!settings.has_value()) {
    return std::nullopt;
  }

  if (is_cloud_analysis()) {
    settings->cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(GetCloudAnalysisSettings(data_region));
  } else {
    settings->cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(GetLocalAnalysisSettings());
  }

  return settings;
}

void AnalysisServiceSettings::ParseSourceDestinationPatternSettings(
    const base::Value::List* pattern_settings_list,
    bool is_enabled_pattern) {
  if (!pattern_settings_list || pattern_settings_list->empty()) {
    return;
  }

  for (const base::Value& pattern_setting : *pattern_settings_list) {
    const base::Value::Dict* pattern_dict = pattern_setting.GetIfDict();
    if (!pattern_dict) {
      continue;
    }

    auto* url_list = pattern_dict->FindList(kKeyUrlList);
    auto* source_destination_list =
        pattern_dict->FindList(kKeySourceDestinationList);

    if (url_list && source_destination_list) {
      DLOG(ERROR) << kKeyUrlList << " and " << kKeySourceDestinationList
                  << " specified together. Ignoring it.";
    } else if (source_destination_list) {
      AddSourceDestinationSettings(*pattern_dict, is_enabled_pattern);
    }
  }
}

void AnalysisServiceSettings::AddSourceDestinationSettings(
    const base::Value::Dict& source_destination_settings_value,
    bool enabled) {
  CHECK(analysis_config_);
  CHECK(source_destination_matcher_);
  if (enabled) {
    CHECK(disabled_patterns_settings_.empty());
  } else {
    CHECK(!enabled_patterns_settings_.empty());
  }

  URLPatternSettings setting;

  const base::Value::List* tags =
      source_destination_settings_value.FindList(kKeyTags);
  if (!tags) {
    return;
  }

  for (const base::Value& tag : *tags) {
    if (tag.is_string()) {
      for (const auto& supported_tag : analysis_config_->supported_tags) {
        if (tag.GetString() == supported_tag.name) {
          setting.tags.insert(tag.GetString());
        }
      }
    }
  }

  // Add the source destination rules to the source_destination_matcher and
  // store the condition set IDs.
  const base::Value::List* source_destination_list =
      source_destination_settings_value.FindList(kKeySourceDestinationList);
  if (!source_destination_list) {
    return;
  }

  base::MatcherStringPattern::ID previous_id = id_;
  source_destination_matcher_->AddFilters(&id_, source_destination_list);
  if (previous_id == id_) {
    // No rules were added, so don't save settings, as they would override other
    // valid settings.
    return;
  }

  if (enabled) {
    enabled_patterns_settings_[id_] = std::move(setting);
  } else {
    disabled_patterns_settings_[id_] = std::move(setting);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

AnalysisServiceSettings::AnalysisServiceSettings(AnalysisServiceSettings&&) =
    default;
AnalysisServiceSettings& AnalysisServiceSettings::operator=(
    AnalysisServiceSettings&&) = default;
AnalysisServiceSettings::~AnalysisServiceSettings() = default;

}  // namespace enterprise_connectors
