// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace policy {

namespace {

using RuleId = DlpRulesManagerImpl::RuleId;

using UrlConditionId = DlpRulesManagerImpl::UrlConditionId;

using RulesConditionsMap = std::map<RuleId, UrlConditionId>;

template <typename T>
struct MatchedRuleInfo {
  MatchedRuleInfo(DlpRulesManager::Level level,
                  absl::optional<RuleId> rule_id,
                  absl::optional<T> url_condition)
      : level(level), rule_id(rule_id), url_condition(url_condition) {}
  MatchedRuleInfo(const MatchedRuleInfo&) = default;
  MatchedRuleInfo() = default;
  MatchedRuleInfo& operator=(const MatchedRuleInfo&) = default;
  ~MatchedRuleInfo() = default;

  DlpRulesManager::Level level;
  absl::optional<RuleId> rule_id;
  absl::optional<T> url_condition;
};

constexpr char kWildCardMatching[] = "*";

DlpRulesManager::Restriction GetClassMapping(const std::string& restriction) {
  static constexpr auto kRestrictionsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Restriction>(
          {{dlp::kClipboardRestriction,
            DlpRulesManager::Restriction::kClipboard},
           {dlp::kScreenshotRestriction,
            DlpRulesManager::Restriction::kScreenshot},
           {dlp::kPrintingRestriction, DlpRulesManager::Restriction::kPrinting},
           {dlp::kPrivacyScreenRestriction,
            DlpRulesManager::Restriction::kPrivacyScreen},
           {dlp::kScreenShareRestriction,
            DlpRulesManager::Restriction::kScreenShare},
           {dlp::kFilesRestriction, DlpRulesManager::Restriction::kFiles}});

  auto* it = kRestrictionsMap.find(restriction);
  return (it == kRestrictionsMap.end())
             ? DlpRulesManager::Restriction::kUnknownRestriction
             : it->second;
}

DlpRulesManager::Level GetLevelMapping(const std::string& level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Level>(
          {{dlp::kAllowLevel, DlpRulesManager::Level::kAllow},
           {dlp::kBlockLevel, DlpRulesManager::Level::kBlock},
           {dlp::kWarnLevel, DlpRulesManager::Level::kWarn},
           {dlp::kReportLevel, DlpRulesManager::Level::kReport}});
  auto* it = kLevelsMap.find(level);
  return (it == kLevelsMap.end()) ? DlpRulesManager::Level::kNotSet
                                  : it->second;
}

DlpRulesManager::Component GetComponentMapping(const std::string& component) {
  static constexpr auto kComponentsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Component>(
          {{dlp::kArc, DlpRulesManager::Component::kArc},
           {dlp::kCrostini, DlpRulesManager::Component::kCrostini},
           {dlp::kPluginVm, DlpRulesManager::Component::kPluginVm},
           {dlp::kDrive, DlpRulesManager::Component::kDrive},
           {dlp::kUsb, DlpRulesManager::Component::kUsb}});

  auto* it = kComponentsMap.find(component);
  return (it == kComponentsMap.end())
             ? DlpRulesManager::Component::kUnknownComponent
             : it->second;
}

::dlp::DlpComponent GetComponentProtoMapping(const std::string& component) {
  static constexpr auto kComponentsMap =
      base::MakeFixedFlatMap<base::StringPiece, ::dlp::DlpComponent>(
          {{dlp::kArc, ::dlp::DlpComponent::ARC},
           {dlp::kCrostini, ::dlp::DlpComponent::CROSTINI},
           {dlp::kPluginVm, ::dlp::DlpComponent::PLUGIN_VM},
           {dlp::kDrive, ::dlp::DlpComponent::GOOGLE_DRIVE},
           {dlp::kUsb, ::dlp::DlpComponent::USB}});

  auto* it = kComponentsMap.find(component);
  return (it == kComponentsMap.end()) ? ::dlp::DlpComponent::UNKNOWN_COMPONENT
                                      : it->second;
}

// Creates `urls` conditions, saves patterns strings mapping in
// `patterns_mapping`, and saves conditions ids to rules ids mapping in `map`.
void AddUrlConditions(url_matcher::URLMatcher* matcher,
                      UrlConditionId& condition_id,
                      const base::Value::List* urls,
                      url_matcher::URLMatcherConditionSet::Vector& conditions,
                      std::map<UrlConditionId, std::string>& patterns_mapping,
                      RuleId rule_id,
                      std::map<UrlConditionId, RuleId>& map) {
  DCHECK(urls);
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  bool match_subdomains = true;
  for (const auto& list_entry : *urls) {
    std::string url = list_entry.GetString();
    if (!url_matcher::util::FilterToComponents(
            url, &scheme, &host, &match_subdomains, &port, &path, &query)) {
      LOG(ERROR) << "Invalid pattern " << url;
      continue;
    }
    auto condition_set = url_matcher::util::CreateConditionSet(
        matcher, ++condition_id, scheme, host, match_subdomains, port, path,
        query, /*allow=*/true);

    conditions.push_back(std::move(condition_set));
    map[condition_id] = rule_id;
    patterns_mapping[condition_id] = url;
  }
}

// Matches `url` against `url_matcher` patterns and returns the rules IDs
// configured with the matched patterns.
RulesConditionsMap MatchUrlAndGetRulesMapping(
    const GURL& url,
    const url_matcher::URLMatcher* url_matcher,
    const std::map<UrlConditionId, RuleId>& rules_map) {
  DCHECK(url_matcher);
  const std::set<UrlConditionId> url_conditions_ids =
      url_matcher->MatchURL(url);

  RulesConditionsMap rules_conditions_map;
  for (const auto& id : url_conditions_ids) {
    rules_conditions_map[rules_map.at(id)] = id;
  }
  return rules_conditions_map;
}

// Determines the maximum level of the rules of given
// `restriction` joined with the `selected_rules`, and returns MatchedRuleInfo
// of the matched rule.
template <typename T>
MatchedRuleInfo<T> GetMaxJoinRestrictionLevelAndRuleId(
    const DlpRulesManager::Restriction restriction,
    const std::map<RuleId, T>& selected_rules,
    const std::map<DlpRulesManager::Restriction,
                   std::map<RuleId, DlpRulesManager::Level>>& restrictions_map,
    const bool ignore_allow = false) {
  auto restriction_it = restrictions_map.find(restriction);
  if (restriction_it == restrictions_map.end()) {
    return MatchedRuleInfo<T>(DlpRulesManager::Level::kAllow, absl::nullopt,
                              absl::nullopt);
  }

  const std::map<RuleId, DlpRulesManager::Level>& restriction_rules =
      restriction_it->second;

  DlpRulesManager::Level max_level = DlpRulesManager::Level::kNotSet;
  absl::optional<T> url_condition = absl::nullopt;
  absl::optional<RuleId> matched_rule_id = absl::nullopt;

  for (const auto& rule_pair : selected_rules) {
    const auto& restriction_rule_itr = restriction_rules.find(rule_pair.first);
    if (restriction_rule_itr == restriction_rules.end()) {
      continue;
    }
    if (ignore_allow &&
        restriction_rule_itr->second == DlpRulesManager::Level::kAllow) {
      continue;
    }
    if (restriction_rule_itr->second > max_level) {
      max_level = restriction_rule_itr->second;
      url_condition = rule_pair.second;
      matched_rule_id = rule_pair.first;
    }
  }

  if (max_level == DlpRulesManager::Level::kNotSet) {
    return MatchedRuleInfo<T>(DlpRulesManager::Level::kAllow, absl::nullopt,
                              absl::nullopt);
  }

  return MatchedRuleInfo(max_level, matched_rule_id, url_condition);
}

void OnSetDlpFilesPolicy(const ::dlp::SetDlpFilesPolicyResponse response) {
  DlpBooleanHistogram(dlp::kErrorsFilesPolicySetup,
                      response.has_error_message());
  if (response.has_error_message()) {
    DlpScopedFileAccessDelegate::DeleteInstance();
    LOG(ERROR) << "Failed to set DLP Files policy and start DLP daemon, error: "
               << response.error_message();
    return;
  }
  DCHECK(chromeos::DlpClient::Get()->IsAlive());
  DlpScopedFileAccessDelegate::Initialize(chromeos::DlpClient::Get());
}

::dlp::DlpRuleLevel GetLevelProtoEnum(const DlpRulesManager::Level level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<DlpRulesManager::Level, ::dlp::DlpRuleLevel>(
          {{DlpRulesManager::Level::kNotSet, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kReport, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kWarn, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kBlock, ::dlp::DlpRuleLevel::BLOCK},
           {DlpRulesManager::Level::kAllow, ::dlp::DlpRuleLevel::ALLOW}});
  return kLevelsMap.at(level);
}

}  // namespace

DlpRulesManagerImpl::~DlpRulesManagerImpl() {
  DataTransferDlpController::DeleteInstance();
  DlpScopedFileAccessDelegate::DeleteInstance();
}

// static
void DlpRulesManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(policy_prefs::kDlpReportingEnabled, false);
  registry->RegisterListPref(policy_prefs::kDlpRulesList);
  registry->RegisterIntegerPref(policy_prefs::kDlpClipboardCheckSizeLimit, 0);
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestricted(
    const GURL& source,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kPrinting ||
         restriction == Restriction::kPrivacyScreen ||
         restriction == Restriction::kScreenshot ||
         restriction == Restriction::kScreenShare);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  return GetMaxJoinRestrictionLevelAndRuleId(restriction, src_rules_map,
                                             restrictions_map_)
      .level;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedByAnyRule(
    const GURL& source,
    Restriction restriction,
    std::string* out_source_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const MatchedRuleInfo rule_info = GetMaxJoinRestrictionLevelAndRuleId(
      restriction, src_rules_map, restrictions_map_,
      /*ignore_allow=*/true);

  if (rule_info.url_condition.has_value() && out_source_pattern) {
    UrlConditionId src_condition_id = rule_info.url_condition.value();
    *out_source_pattern = src_patterns_mapping_.at(src_condition_id);
  }
  if (rule_info.rule_id.has_value() && out_rule_metadata) {
    auto rule_metadata_itr =
        rules_id_metadata_mapping_.find(rule_info.rule_id.value());
    if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
      *out_rule_metadata = rule_metadata_itr->second;
    }
  }

  return rule_info.level;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedDestination(
    const GURL& source,
    const GURL& destination,
    Restriction restriction,
    std::string* out_source_pattern,
    std::string* out_destination_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  // Allow copy/paste within the same document.
  if (url::IsSameOriginWith(source, destination))
    return Level::kAllow;

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const RulesConditionsMap dst_rules_map = MatchUrlAndGetRulesMapping(
      destination, dst_url_matcher_.get(), dst_url_rules_mapping_);

  std::map<DlpRulesManagerImpl::RuleId,
           std::pair<DlpRulesManagerImpl::UrlConditionId,
                     DlpRulesManagerImpl::UrlConditionId>>
      intersection_rules;
  auto src_map_itr = src_rules_map.begin();
  auto dst_map_itr = dst_rules_map.begin();
  while (src_map_itr != src_rules_map.end() &&
         dst_map_itr != dst_rules_map.end()) {
    if (src_map_itr->first < dst_map_itr->first) {
      ++src_map_itr;
    } else if (dst_map_itr->first < src_map_itr->first) {
      ++dst_map_itr;
    } else {
      intersection_rules.insert(std::make_pair(
          src_map_itr->first,
          std::make_pair(src_map_itr->second, dst_map_itr->second)));
      ++src_map_itr;
      ++dst_map_itr;
    }
  }

  const MatchedRuleInfo rule_info = GetMaxJoinRestrictionLevelAndRuleId(
      restriction, intersection_rules, restrictions_map_);
  if (rule_info.url_condition.has_value() && out_source_pattern &&
      out_destination_pattern) {
    UrlConditionId src_condition_id = rule_info.url_condition.value().first;
    UrlConditionId dst_condition_id = rule_info.url_condition.value().second;
    if (out_source_pattern)
      *out_source_pattern = src_patterns_mapping_.at(src_condition_id);
    if (out_destination_pattern)
      *out_destination_pattern = dst_patterns_mapping_.at(dst_condition_id);
    if (rule_info.rule_id.has_value() && out_rule_metadata) {
      auto rule_metadata_itr =
          rules_id_metadata_mapping_.find(rule_info.rule_id.value());
      if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
        *out_rule_metadata = rule_metadata_itr->second;
      }
    }
  }
  return rule_info.level;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedComponent(
    const GURL& source,
    const Component& destination,
    Restriction restriction,
    std::string* out_source_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  if (destination == Component::kUnknownComponent) {
    return DlpRulesManager::Level::kAllow;
  }

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  auto it = components_rules_.find(destination);
  if (it == components_rules_.end())
    return Level::kAllow;

  const std::set<RuleId>& component_rules_ids = it->second;

  RulesConditionsMap intersection_rules;
  auto src_map_itr = src_rules_map.begin();
  auto component_rules_itr = component_rules_ids.begin();
  while (src_map_itr != src_rules_map.end() &&
         component_rules_itr != component_rules_ids.end()) {
    if (src_map_itr->first < *component_rules_itr) {
      ++src_map_itr;
    } else if (*component_rules_itr < src_map_itr->first) {
      ++component_rules_itr;
    } else {
      intersection_rules.insert(*src_map_itr);
      ++src_map_itr;
      ++component_rules_itr;
    }
  }

  const MatchedRuleInfo rule_info = GetMaxJoinRestrictionLevelAndRuleId(
      restriction, intersection_rules, restrictions_map_);
  if (rule_info.url_condition.has_value() && out_source_pattern) {
    UrlConditionId src_condition_id = rule_info.url_condition.value();
    *out_source_pattern = src_patterns_mapping_.at(src_condition_id);
  }
  if (rule_info.rule_id.has_value() && out_rule_metadata) {
    auto rule_metadata_itr =
        rules_id_metadata_mapping_.find(rule_info.rule_id.value());
    if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
      *out_rule_metadata = rule_metadata_itr->second;
    }
  }
  return rule_info.level;
}

DlpRulesManager::AggregatedDestinations
DlpRulesManagerImpl::GetAggregatedDestinations(const GURL& source,
                                               Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  auto restriction_it = restrictions_map_.find(restriction);
  if (restriction_it == restrictions_map_.end()) {
    return std::map<Level, std::set<std::string>>();
  }
  const std::map<RuleId, DlpRulesManager::Level>& restriction_rules =
      restriction_it->second;

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);
  // We need to check all possible destinations for rules that apply to it and
  // to the `source`. There can be many matching rules, but we want to keep only
  // the highest enforced level for each destination.
  std::map<std::string, Level> destination_level_map;
  // If there's a wildcard for a level, we should ignore all destinations for
  // lower levels.
  Level wildcard_level = Level::kNotSet;
  for (auto dst_map_itr : dst_url_rules_mapping_) {
    auto src_map_itr = src_rules_map.find(dst_map_itr.second);
    if (src_map_itr == src_rules_map.end()) {
      continue;
    }
    const auto& restriction_rule_itr =
        restriction_rules.find(src_map_itr->first);
    if (restriction_rule_itr == restriction_rules.end()) {
      continue;
    }
    UrlConditionId dst_condition_id = dst_map_itr.first;
    std::string destination_pattern =
        dst_patterns_mapping_.at(dst_condition_id);
    Level level = restriction_rule_itr->second;
    auto it = destination_level_map.find(destination_pattern);
    if (it == destination_level_map.end() || level > it->second) {
      destination_level_map[destination_pattern] = restriction_rule_itr->second;
    }
    if (destination_pattern == kWildCardMatching && level > wildcard_level) {
      wildcard_level = level;
    }
  }

  std::map<Level, std::set<std::string>> result;
  for (auto it : destination_level_map) {
    if (it.first == kWildCardMatching) {
      result[it.second] = {it.first};
    } else if (it.second >= wildcard_level &&
               result[it.second].find(kWildCardMatching) ==
                   result[it.second].end()) {
      result[it.second].insert(it.first);
    }
  }

  return result;
}

DlpRulesManager::AggregatedComponents
DlpRulesManagerImpl::GetAggregatedComponents(const GURL& source,
                                             Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  std::map<Level, std::set<Component>> result;
  for (Component component : components) {
    std::string out_source_pattern;
    Level level = IsRestrictedComponent(source, component, restriction,
                                        &out_source_pattern, nullptr);
    result[level].insert(component);
  }

  return result;
}

DlpRulesManagerImpl::DlpRulesManagerImpl(PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManagerImpl::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();

  if (IsReportingEnabled())
    reporting_manager_ = std::make_unique<DlpReportingManager>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::DlpClient::Get()) {
    dlp_client_observation_.Observe(chromeos::DlpClient::Get());
  }
#endif
}

bool DlpRulesManagerImpl::IsReportingEnabled() const {
  return g_browser_process->local_state()->GetBoolean(
      policy_prefs::kDlpReportingEnabled);
}

DlpReportingManager* DlpRulesManagerImpl::GetReportingManager() const {
  return reporting_manager_.get();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
DlpFilesController* DlpRulesManagerImpl::GetDlpFilesController() const {
  return files_controller_.get();
}
#endif

std::string DlpRulesManagerImpl::GetSourceUrlPattern(
    const GURL& source_url,
    Restriction restriction,
    Level level,
    RuleMetadata* out_rule_metadata) const {
  const std::set<UrlConditionId> url_conditions_ids =
      src_url_matcher_->MatchURL(source_url);

  std::map<RuleId, UrlConditionId> rules_conditions_map;
  for (const auto& condition_id : url_conditions_ids) {
    rules_conditions_map.insert(
        std::make_pair(src_url_rules_mapping_.at(condition_id), condition_id));
  }
  auto restriction_itr = restrictions_map_.find(restriction);
  if (restriction_itr == restrictions_map_.end())
    return std::string();

  const auto rules_levels_map = restriction_itr->second;
  for (const auto& rule_level_entry : rules_levels_map) {
    auto rule_id = rule_level_entry.first;
    auto lvl = rule_level_entry.second;
    auto rule_condition_itr = rules_conditions_map.find(rule_id);
    if (lvl == level && rule_condition_itr != rules_conditions_map.end()) {
      auto condition_id = rule_condition_itr->second;
      auto condition_pattern_itr = src_patterns_mapping_.find(condition_id);
      if (condition_pattern_itr != src_patterns_mapping_.end()) {
        if (out_rule_metadata) {
          auto rule_metadata_itr = rules_id_metadata_mapping_.find(rule_id);
          if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
            *out_rule_metadata = rule_metadata_itr->second;
          }
        }
        return condition_pattern_itr->second;
      }
    }
  }
  return std::string();
}

size_t DlpRulesManagerImpl::GetClipboardCheckSizeLimitInBytes() const {
  return pref_change_registrar_.prefs()->GetInteger(
      policy_prefs::kDlpClipboardCheckSizeLimit);
}

bool DlpRulesManagerImpl::IsFilesPolicyEnabled() const {
  return base::FeatureList::IsEnabled(
             features::kDataLeakPreventionFilesRestriction) &&
         base::Contains(restrictions_map_,
                        DlpRulesManager::Restriction::kFiles) &&
         chromeos::DlpClient::Get() && chromeos::DlpClient::Get()->IsAlive();
}

void DlpRulesManagerImpl::DlpDaemonRestarted() {
  // This should trigger re-notification of DLP daemon if needed.
  OnPolicyUpdate();
}

void DlpRulesManagerImpl::OnPolicyUpdate() {
  components_rules_.clear();
  restrictions_map_.clear();
  src_url_rules_mapping_.clear();
  dst_url_rules_mapping_.clear();
  src_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  dst_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  src_patterns_mapping_.clear();
  dst_patterns_mapping_.clear();
  src_conditions_.clear();
  dst_conditions_.clear();
  rules_id_metadata_mapping_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  files_controller_ = nullptr;
#endif

  if (!base::FeatureList::IsEnabled(features::kDataLeakPreventionPolicy)) {
    return;
  }

  const base::Value::List& rules_list =
      g_browser_process->local_state()->GetList(policy_prefs::kDlpRulesList);

  DlpBooleanHistogram(dlp::kDlpPolicyPresentUMA, !rules_list.empty());
  if (rules_list.empty()) {
    DataTransferDlpController::DeleteInstance();
    return;
  }

  RuleId rules_counter = 0;
  UrlConditionId src_url_condition_id = 0;
  UrlConditionId dst_url_condition_id = 0;

  // Constructing request to send the policy to DLP Files daemon.
  ::dlp::SetDlpFilesPolicyRequest request_to_daemon;

  for (const base::Value& rule_value : rules_list) {
    const base::Value::Dict& rule = rule_value.GetDict();
    const base::Value::Dict* sources = rule.FindDict("sources");
    DCHECK(sources);
    const base::Value::List* sources_urls = sources->FindList("urls");
    DCHECK(sources_urls);  // This DCHECK should be removed when other types are
                           // supported as sources.

    AddUrlConditions(src_url_matcher_.get(), src_url_condition_id, sources_urls,
                     src_conditions_, src_patterns_mapping_, rules_counter,
                     src_url_rules_mapping_);

    const base::Value::Dict* destinations = rule.FindDict("destinations");
    const base::Value::List* destinations_urls =
        destinations ? destinations->FindList("urls") : nullptr;
    if (destinations_urls) {
      AddUrlConditions(dst_url_matcher_.get(), dst_url_condition_id,
                       destinations_urls, dst_conditions_,
                       dst_patterns_mapping_, rules_counter,
                       dst_url_rules_mapping_);
    }
    const base::Value::List* destinations_components =
        destinations ? destinations->FindList("components") : nullptr;
    if (destinations_components) {
      for (const auto& component : *destinations_components) {
        DCHECK(component.is_string());
        components_rules_[GetComponentMapping(component.GetString())].insert(
            rules_counter);
      }
    }

    const std::string* rule_name = rule.FindString("name");
    const std::string* rule_id = rule.FindString("rule_id");
    // Only add to metadata if both fields are set, so we can control behaviour
    // from the server side.
    if (rule_name && rule_id) {
      rules_id_metadata_mapping_.emplace(rules_counter,
                                         RuleMetadata(*rule_name, *rule_id));
    }

    const base::Value::List* restrictions = rule.FindList("restrictions");
    DCHECK(restrictions);
    for (const auto& restriction_value : *restrictions) {
      const base::Value::Dict& restriction = restriction_value.GetDict();
      const std::string* rule_class_str = restriction.FindString("class");
      DCHECK(rule_class_str);
      const std::string* rule_level_str = restriction.FindString("level");
      DCHECK(rule_level_str);

      const Restriction rule_restriction = GetClassMapping(*rule_class_str);
      if (rule_restriction == Restriction::kUnknownRestriction)
        continue;

      Level rule_level = GetLevelMapping(*rule_level_str);
      if (rule_level == Level::kNotSet)
        continue;

      bool rule_has_destinations =
          destinations_urls && !destinations_urls->empty();
      bool rule_has_components =
          destinations_components && !destinations_components->empty();

      if (rule_restriction == Restriction::kFiles &&
          (rule_has_destinations || rule_has_components)) {
        ::dlp::DlpFilesRule files_rule;
        for (const auto& url : *sources_urls) {
          DCHECK(url.is_string());
          files_rule.add_source_urls(url.GetString());
        }

        if (rule_has_destinations) {
          for (const auto& url : *destinations_urls) {
            DCHECK(url.is_string());
            files_rule.add_destination_urls(url.GetString());
          }
        }

        if (rule_has_components) {
          for (const auto& component : *destinations_components) {
            DCHECK(component.is_string());
            files_rule.add_destination_components(
                GetComponentProtoMapping(component.GetString()));
          }
        }

        files_rule.set_level(GetLevelProtoEnum(rule_level));
        request_to_daemon.mutable_rules()->Add(std::move(files_rule));
      }

      DlpRestrictionConfiguredHistogram(rule_restriction);
      restrictions_map_[rule_restriction].emplace(rules_counter, rule_level);
    }
    ++rules_counter;
  }

  src_url_matcher_->AddConditionSets(src_conditions_);
  dst_url_matcher_->AddConditionSets(dst_conditions_);
  if (base::Contains(restrictions_map_, Restriction::kClipboard)
  // TODO(b/269610458): It should be instantiated for files in
  // Lacros as well.
#if BUILDFLAG(IS_CHROMEOS_ASH)
      || (base::FeatureList::IsEnabled(
              features::kDataLeakPreventionFilesRestriction) &&
          request_to_daemon.rules_size() > 0)
#endif
  ) {
    DataTransferDlpController::Init(*this);
  } else {
    DataTransferDlpController::DeleteInstance();
  }

  if (base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    if (request_to_daemon.rules_size() > 0) {
      // Start and/or activate the daemon.
      DlpBooleanHistogram(dlp::kFilesDaemonStartedUMA, true);
      chromeos::DlpClient::Get()->SetDlpFilesPolicy(
          request_to_daemon, base::BindOnce(&OnSetDlpFilesPolicy));
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (!files_controller_) {
        files_controller_ = std::make_unique<DlpFilesController>(*this);
      }
#endif
    } else if (chromeos::DlpClient::Get() &&
               chromeos::DlpClient::Get()->IsAlive()) {
      // The daemon is running, but should be deactivated by sending empty
      // policy.
      chromeos::DlpClient::Get()->SetDlpFilesPolicy(
          request_to_daemon, base::BindOnce(&OnSetDlpFilesPolicy));
    } else {
      // The daemon is not running and should not be communicated.
      DlpScopedFileAccessDelegate::DeleteInstance();
    }
  }
}

}  // namespace policy
