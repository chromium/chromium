// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/url_matcher/url_util.h"

namespace enterprise_connectors {

AnalysisServiceSettings::AnalysisServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there, and it should match
  // an existing provider.
  const std::string* service_provider_name =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider_name) {
    service_provider_name_ = *service_provider_name;
    service_provider_ =
        service_provider_config.GetServiceProvider(*service_provider_name);
    if (!service_provider_)
      return;
  } else {
    return;
  }

  // Add the patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::URLMatcherConditionSet::ID id(0);
  const base::Value* enable = settings_value.FindListKey(kKeyEnable);
  if (enable && enable->is_list() && !enable->GetListDeprecated().empty()) {
    for (const base::Value& value : enable->GetListDeprecated())
      AddUrlPatternSettings(value, true, &id);
  } else {
    return;
  }

  const base::Value* disable = settings_value.FindListKey(kKeyDisable);
  if (disable && disable->is_list()) {
    for (const base::Value& value : disable->GetListDeprecated())
      AddUrlPatternSettings(value, false, &id);
  }

  // The block settings are optional, so a default is used if they can't be
  // found.
  block_until_verdict_ =
      settings_value.FindIntKey(kKeyBlockUntilVerdict).value_or(0)
          ? BlockUntilVerdict::BLOCK
          : BlockUntilVerdict::NO_BLOCK;
  block_password_protected_files_ =
      settings_value.FindBoolKey(kKeyBlockPasswordProtected).value_or(false);
  block_large_files_ =
      settings_value.FindBoolKey(kKeyBlockLargeFiles).value_or(false);
  block_unsupported_file_types_ =
      settings_value.FindBoolKey(kKeyBlockUnsupportedFileTypes).value_or(false);
  minimum_data_size_ =
      settings_value.FindIntKey(kKeyMinimumDataSize).value_or(100);

  const base::Value* custom_messages =
      settings_value.FindListKey(kKeyCustomMessages);
  if (custom_messages && custom_messages->is_list()) {
    for (const base::Value& value : custom_messages->GetListDeprecated()) {
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

      custom_message_data_[*tag] = data;
    }
  }

  const base::Value* require_justification_tags =
      settings_value.FindListKey(kKeyRequireJustificationTags);
  if (require_justification_tags && require_justification_tags->is_list()) {
    for (const base::Value& tag :
         require_justification_tags->GetListDeprecated()) {
      tags_requiring_justification_.insert(tag.GetString());
    }
  }
}

// static
absl::optional<AnalysisServiceSettings::URLPatternSettings>
AnalysisServiceSettings::GetPatternSettings(
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

  AnalysisSettings settings;

  settings.tags = std::move(tags);
  settings.block_until_verdict = block_until_verdict_;
  settings.block_password_protected_files = block_password_protected_files_;
  settings.block_large_files = block_large_files_;
  settings.block_unsupported_file_types = block_unsupported_file_types_;
  settings.analysis_url = GURL(service_provider_->analysis_url());
  DCHECK(settings.analysis_url.is_valid());
  settings.minimum_data_size = minimum_data_size_;
  settings.custom_message_data = custom_message_data_;
  settings.tags_requiring_justification = tags_requiring_justification_;

  return settings;
}

bool AnalysisServiceSettings::ShouldBlockUntilVerdict() const {
  if (!IsValid())
    return false;
  return block_until_verdict_ == BlockUntilVerdict::BLOCK;
}

absl::optional<std::u16string> AnalysisServiceSettings::GetCustomMessage(
    const std::string& tag) {
  const auto& element = custom_message_data_.find(tag);

  if (!IsValid() || element == custom_message_data_.end() ||
      element->second.message.empty()) {
    return absl::nullopt;
  }

  return element->second.message;
}

absl::optional<GURL> AnalysisServiceSettings::GetLearnMoreUrl(
    const std::string& tag) {
  const auto& element = custom_message_data_.find(tag);

  if (!IsValid() || element == custom_message_data_.end() ||
      element->second.learn_more_url.is_empty()) {
    return absl::nullopt;
  }

  return element->second.learn_more_url;
}

absl::optional<bool> AnalysisServiceSettings::GetBypassJustificationRequired(
    const std::string& tag) {
  return tags_requiring_justification_.find(tag) !=
         tags_requiring_justification_.end();
}

void AnalysisServiceSettings::AddUrlPatternSettings(
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

  const base::Value* tags = url_settings_value.FindListKey(kKeyTags);
  if (!tags)
    return;

  for (const base::Value& tag : tags->GetListDeprecated()) {
    if (tag.is_string() &&
        (service_provider_->analysis_tags().count(tag.GetString()) == 1)) {
      setting.tags.insert(tag.GetString());
    }
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value* url_list = url_settings_value.FindListKey(kKeyUrlList);
  if (!url_list)
    return;

  url_matcher::util::AddFilters(matcher_.get(), enabled, id,
                                &base::Value::AsListValue(*url_list));

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);
}

std::set<std::string> AnalysisServiceSettings::GetTags(
    const std::set<url_matcher::URLMatcherConditionSet::ID>& matches) const {
  std::set<std::string> enable_tags;
  std::set<std::string> disable_tags;
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
    auto tags = std::move(maybe_pattern_setting.value().tags);
    if (enable)
      enable_tags.insert(tags.begin(), tags.end());
    else
      disable_tags.insert(tags.begin(), tags.end());
  }

  for (const std::string& tag_to_disable : disable_tags)
    enable_tags.erase(tag_to_disable);

  return enable_tags;
}

bool AnalysisServiceSettings::IsValid() const {
  // The settings are invalid if no provider was given.
  if (!service_provider_)
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
