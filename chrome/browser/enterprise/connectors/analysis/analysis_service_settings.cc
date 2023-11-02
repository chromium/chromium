// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/url_matcher/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"
#endif

namespace enterprise_connectors {

AnalysisServiceSettings::AnalysisServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;
  const base::Value::Dict& settings_dict = settings_value.GetDict();

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_dict.FindString(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_name_ = *service_provider_name;
    if (service_provider_config.count(service_provider_name_)) {
      analysis_config_ =
          service_provider_config.at(service_provider_name_).analysis;
    }
    if (!analysis_config_) {
      DLOG(ERROR) << "No analysis config for corresponding service provider";
      return;
    }
  } else {
    return;
  }

  // Add the patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  matcher_ = std::make_unique<url_matcher::URLMatcher>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  source_destination_matcher_ = std::make_unique<SourceDestinationMatcherAsh>();
#endif
  base::MatcherStringPattern::ID id(0);
  for (auto [key, is_enable] :
       {std::pair{kKeyEnable, true}, {kKeyDisable, false}}) {
    const base::Value::List* list = settings_dict.FindList(key);
    if (list && !list->empty()) {
      for (const base::Value& value : *list) {
        const base::Value::Dict* dict = value.GetIfDict();
        if (!dict) {
          continue;
        }
        auto* url_list = dict->FindList(kKeyUrlList);
        auto* source_destination_list =
            dict->FindList(kKeySourceDestinationList);
        if (url_list && source_destination_list) {
          DLOG(ERROR) << kKeyUrlList << " and " << kKeySourceDestinationList
                      << " specified together. Ignoring it.";
        } else if (url_list) {
          AddUrlPatternSettings(*dict, is_enable, &id);
        } else if (source_destination_list) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
          AddSourceDestinationSettings(*dict, is_enable, &id);
#else
          DLOG(ERROR) << kKeySourceDestinationList
                      << " specified on unsupported platform. Ignoring it.";
#endif
        } else {
          DLOG(ERROR) << "Neither " << kKeyUrlList << " nor "
                      << kKeySourceDestinationList
                      << " found in analysis settings. Ignoring it.";
        }
      }
    } else if (is_enable) {
      // If nothing is enabled, just return and don't parse anything else.
      return;
    }
  }

  // The block settings are optional, so a default is used if they can't be
  // found.
  block_until_verdict_ =
      settings_dict.FindInt(kKeyBlockUntilVerdict).value_or(0)
          ? BlockUntilVerdict::kBlock
          : BlockUntilVerdict::kNoBlock;
  block_password_protected_files_ =
      settings_dict.FindBool(kKeyBlockPasswordProtected).value_or(false);
  block_large_files_ =
      settings_dict.FindBool(kKeyBlockLargeFiles).value_or(false);
  block_unsupported_file_types_ =
      settings_dict.FindBool(kKeyBlockUnsupportedFileTypes).value_or(false);
  minimum_data_size_ = settings_dict.FindInt(kKeyMinimumDataSize).value_or(100);

  const base::Value::List* custom_messages =
      settings_dict.FindList(kKeyCustomMessages);
  if (custom_messages) {
    for (const base::Value& value : *custom_messages) {
      // As of now, this list will contain one message per tag. At some point,
      // the server may start sending one message per language/tag pair. If this
      // is the case, this code should be changed to match the language to
      // Chrome's UI language.
      const std::string* tag = value.FindStringKey(kKeyCustomMessagesTag);
      if (!tag)
        continue;

      CustomMessageData data;

      const std::string* message =
          value.FindStringKey(kKeyCustomMessagesMessage);
      // This string originates as a protobuf string on the server, which are
      // utf8 and it's used in the UI where it needs to be encoded as utf16. Do
      // the conversion now, otherwise code down the line may not be able to
      // determine if the std::string is ASCII or UTF8 before passing it to the
      // UI.
      data.message = base::UTF8ToUTF16(message ? *message : "");

      const std::string* url =
          value.FindStringKey(kKeyCustomMessagesLearnMoreUrl);
      data.learn_more_url = url ? GURL(*url) : GURL();

      tags_[*tag].custom_message = std::move(data);
    }
  }

  const base::Value::List* require_justification_tags =
      settings_dict.FindList(kKeyRequireJustificationTags);
  if (require_justification_tags) {
    for (const base::Value& tag : *require_justification_tags) {
      tags_[tag.GetString()].requires_justification = true;
    }
  }

#if BUILDFLAG(IS_WIN)
  const char* verification_key = kKeyWindowsVerification;
#elif BUILDFLAG(IS_MAC)
  const char* verification_key = kKeyMacVerification;
#elif BUILDFLAG(IS_LINUX)
  const char* verification_key = kKeyLinuxVerification;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  const base::Value::Dict& dict = settings_value.GetDict();
  const base::Value::List* signatures =
      dict.FindListByDottedPath(verification_key);
  if (signatures) {
    for (auto& v : *signatures) {
      if (v.is_string())
        verification_signatures_.push_back(v.GetString());
    }
  }
#endif
}

// static
absl::optional<AnalysisServiceSettings::URLPatternSettings>
AnalysisServiceSettings::GetPatternSettings(
    const PatternSettings& patterns,
    base::MatcherStringPattern::ID match) {
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

AnalysisSettings AnalysisServiceSettings::GetAnalysisSettingsWithTags(
    std::map<std::string, TagSettings> tags) const {
  DCHECK(IsValid());

  AnalysisSettings settings;

  settings.block_until_verdict = block_until_verdict_;
  settings.block_password_protected_files = block_password_protected_files_;
  settings.block_large_files = block_large_files_;
  settings.block_unsupported_file_types = block_unsupported_file_types_;
  if (analysis_config_->url) {
    CloudAnalysisSettings cloud_settings;
    cloud_settings.analysis_url = GURL(analysis_config_->url);
    DCHECK(cloud_settings.analysis_url.is_valid());
    settings.cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(std::move(cloud_settings));
  } else {
    DCHECK(analysis_config_->local_path);
    LocalAnalysisSettings local_settings;
    local_settings.local_path = analysis_config_->local_path;
    local_settings.user_specific = analysis_config_->user_specific;
    local_settings.verification_signatures = verification_signatures_;

    settings.cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(std::move(local_settings));
  }
  settings.minimum_data_size = minimum_data_size_;
  settings.tags = std::move(tags);
  return settings;
}

absl::optional<AnalysisSettings> AnalysisServiceSettings::GetAnalysisSettings(
    const GURL& url) const {
  if (!IsValid())
    return absl::nullopt;

  DCHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty())
    return absl::nullopt;

  auto tags = GetTags(matches);
  if (tags.empty())
    return absl::nullopt;

  return GetAnalysisSettingsWithTags(std::move(tags));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<AnalysisSettings> AnalysisServiceSettings::GetAnalysisSettings(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) const {
  if (!IsValid())
    return absl::nullopt;
  DCHECK(source_destination_matcher_);

  auto matches =
      source_destination_matcher_->Match(context, source_url, destination_url);
  if (matches.empty())
    return absl::nullopt;

  auto tags = GetTags(matches);
  if (tags.empty())
    return absl::nullopt;

  return GetAnalysisSettingsWithTags(std::move(tags));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool AnalysisServiceSettings::ShouldBlockUntilVerdict() const {
  if (!IsValid())
    return false;
  return block_until_verdict_ == BlockUntilVerdict::kBlock;
}

absl::optional<std::u16string> AnalysisServiceSettings::GetCustomMessage(
    const std::string& tag) {
  const auto& element = tags_.find(tag);

  if (!IsValid() || element == tags_.end() ||
      element->second.custom_message.message.empty()) {
    return absl::nullopt;
  }

  return element->second.custom_message.message;
}

absl::optional<GURL> AnalysisServiceSettings::GetLearnMoreUrl(
    const std::string& tag) {
  const auto& element = tags_.find(tag);

  if (!IsValid() || element == tags_.end() ||
      element->second.custom_message.learn_more_url.is_empty()) {
    return absl::nullopt;
  }

  return element->second.custom_message.learn_more_url;
}

bool AnalysisServiceSettings::GetBypassJustificationRequired(
    const std::string& tag) {
  return tags_.find(tag) != tags_.end() && tags_.at(tag).requires_justification;
}

void AnalysisServiceSettings::AddUrlPatternSettings(
    const base::Value::Dict& url_settings_dict,
    bool enabled,
    base::MatcherStringPattern::ID* id) {
  DCHECK(id);
  DCHECK(analysis_config_);
  if (enabled)
    DCHECK(disabled_patterns_settings_.empty());
  else
    DCHECK(!enabled_patterns_settings_.empty());

  URLPatternSettings setting;

  const base::Value::List* tags = url_settings_dict.FindList(kKeyTags);
  if (!tags)
    return;

  for (const base::Value& tag : *tags) {
    if (tag.is_string()) {
      for (const auto& supported_tag : analysis_config_->supported_tags) {
        if (tag.GetString() == supported_tag.name)
          setting.tags.insert(tag.GetString());
      }
    }
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value::List* url_list = url_settings_dict.FindList(kKeyUrlList);
  if (!url_list) {
    return;
  }
  base::MatcherStringPattern::ID previous_id = *id;
  url_matcher::util::AddFilters(matcher_.get(), enabled, id, *url_list);

  if (previous_id == *id) {
    // No rules were added, so don't save settings, as they would override other
    // valid settings.
    return;
  }

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AnalysisServiceSettings::AddSourceDestinationSettings(
    const base::Value::Dict& source_destination_settings_value,
    bool enabled,
    base::MatcherStringPattern::ID* id) {
  DCHECK(id);
  DCHECK(analysis_config_);
  DCHECK(source_destination_matcher_);
  if (enabled)
    DCHECK(disabled_patterns_settings_.empty());
  else
    DCHECK(!enabled_patterns_settings_.empty());

  URLPatternSettings setting;

  const base::Value::List* tags =
      source_destination_settings_value.FindList(kKeyTags);
  if (!tags)
    return;

  for (const base::Value& tag : *tags) {
    if (tag.is_string()) {
      for (const auto& supported_tag : analysis_config_->supported_tags) {
        if (tag.GetString() == supported_tag.name)
          setting.tags.insert(tag.GetString());
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

  base::MatcherStringPattern::ID previous_id = *id;
  source_destination_matcher_->AddFilters(id, source_destination_list);
  if (previous_id == *id) {
    // No rules were added, so don't save settings, as they would override other
    // valid settings.
    return;
  }

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::map<std::string, TagSettings> AnalysisServiceSettings::GetTags(
    const std::set<base::MatcherStringPattern::ID>& matches) const {
  std::set<std::string> enable_tags;
  std::set<std::string> disable_tags;
  for (const base::MatcherStringPattern::ID match : matches) {
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
    auto tags = std::move(maybe_pattern_setting.value().tags);
    if (enable)
      enable_tags.insert(tags.begin(), tags.end());
    else
      disable_tags.insert(tags.begin(), tags.end());
  }

  for (const std::string& tag_to_disable : disable_tags)
    enable_tags.erase(tag_to_disable);

  std::map<std::string, TagSettings> output;
  for (const std::string& tag : enable_tags) {
    if (tags_.count(tag))
      output[tag] = tags_.at(tag);
    else
      output[tag] = TagSettings();
  }

  return output;
}

bool AnalysisServiceSettings::IsValid() const {
  // The settings are invalid if no provider was given.
  if (!analysis_config_)
    return false;

  // The settings are invalid if no enabled pattern(s) exist since that would
  // imply no URL can ever have an analysis.
  if (enabled_patterns_settings_.empty())
    return false;

  return true;
}

AnalysisServiceSettings::AnalysisServiceSettings(AnalysisServiceSettings&&) =
    default;
AnalysisServiceSettings::~AnalysisServiceSettings() = default;

AnalysisServiceSettings::URLPatternSettings::URLPatternSettings() = default;
AnalysisServiceSettings::URLPatternSettings::URLPatternSettings(
    const AnalysisServiceSettings::URLPatternSettings&) = default;
AnalysisServiceSettings::URLPatternSettings::URLPatternSettings(
    AnalysisServiceSettings::URLPatternSettings&&) = default;
AnalysisServiceSettings::URLPatternSettings&
AnalysisServiceSettings::URLPatternSettings::operator=(
    const AnalysisServiceSettings::URLPatternSettings&) = default;
AnalysisServiceSettings::URLPatternSettings&
AnalysisServiceSettings::URLPatternSettings::operator=(
    AnalysisServiceSettings::URLPatternSettings&&) = default;
AnalysisServiceSettings::URLPatternSettings::~URLPatternSettings() = default;

}  // namespace enterprise_connectors
