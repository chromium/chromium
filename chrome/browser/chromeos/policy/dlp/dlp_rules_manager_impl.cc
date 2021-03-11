// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace {

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
           {dlp::kWarnLevel, DlpRulesManager::Level::kWarn}});
  auto* it = kLevelsMap.find(level);
  return (it == kLevelsMap.end()) ? DlpRulesManager::Level::kNotSet
                                  : it->second;
}

DlpRulesManager::Component GetComponentMapping(const std::string& component) {
  static constexpr auto kComponentsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Component>(
          {{dlp::kArc, DlpRulesManager::Component::kArc},
           {dlp::kCrostini, DlpRulesManager::Component::kCrostini},
           {dlp::kPluginVm, DlpRulesManager::Component::kPluginVm}});

  auto* it = kComponentsMap.find(component);
  return (it == kComponentsMap.end())
             ? DlpRulesManager::Component::kUnknownComponent
             : it->second;
}

uint8_t GetPriorityMapping(const DlpRulesManager::Level level) {
  static constexpr auto kPrioritiesMap =
      base::MakeFixedFlatMap<DlpRulesManager::Level, uint8_t>(
          {{DlpRulesManager::Level::kNotSet, 0},
           {DlpRulesManager::Level::kWarn, 1},
           {DlpRulesManager::Level::kBlock, 2},
           {DlpRulesManager::Level::kAllow, 3}});
  return kPrioritiesMap.at(level);
}

DlpRulesManager::Level GetMaxLevel(const DlpRulesManager::Level& level_1,
                                   const DlpRulesManager::Level& level_2) {
  return GetPriorityMapping(level_1) > GetPriorityMapping(level_2) ? level_1
                                                                   : level_2;
}

// Inserts a mapping between URLs conditions IDs range to `rule_id` in `map`.
void InsertUrlsRulesMapping(
    DlpRulesManagerImpl::UrlConditionId url_condition_id_start,
    DlpRulesManagerImpl::UrlConditionId url_condition_id_end,
    DlpRulesManagerImpl::RuleId rule_id,
    std::map<DlpRulesManagerImpl::UrlConditionId, DlpRulesManagerImpl::RuleId>&
        map) {
  for (auto url_condition_id = url_condition_id_start;
       url_condition_id <= url_condition_id_end; ++url_condition_id) {
    map[url_condition_id] = rule_id;
  }
}

// Matches `url` against `url_matcher` patterns and returns the rules IDs
// configured with the matched patterns.
std::set<DlpRulesManagerImpl::RuleId> MatchUrlAndGetRulesMapping(
    const GURL& url,
    const url_matcher::URLMatcher* url_matcher,
    const std::map<DlpRulesManagerImpl::UrlConditionId,
                   DlpRulesManagerImpl::RuleId>& rules_map) {
  DCHECK(url_matcher);
  const std::set<DlpRulesManagerImpl::UrlConditionId> url_conditions_ids =
      url_matcher->MatchURL(url);

  std::set<DlpRulesManagerImpl::RuleId> rule_ids;
  for (const auto& id : url_conditions_ids) {
    rule_ids.insert(rules_map.at(id));
  }
  return rule_ids;
}

void OnSetDlpFilesPolicy(const ::dlp::SetDlpFilesPolicyResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to set DLP Files policy and start DLP daemon, error: "
               << response.error_message();
  }
}

::dlp::DlpRuleLevel GetLevelProtoEnum(const DlpRulesManager::Level level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<DlpRulesManager::Level, ::dlp::DlpRuleLevel>(
          {{DlpRulesManager::Level::kNotSet, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kWarn, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kBlock, ::dlp::DlpRuleLevel::BLOCK},
           {DlpRulesManager::Level::kAllow, ::dlp::DlpRuleLevel::ALLOW}});
  return kLevelsMap.at(level);
}

}  // namespace

DlpRulesManagerImpl::~DlpRulesManagerImpl() {
  DataTransferDlpController::DeleteInstance();
}

// static
void DlpRulesManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(policy_prefs::kDlpRulesList);
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestricted(
    const GURL& source,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kPrinting ||
         restriction == Restriction::kPrivacyScreen ||
         restriction == Restriction::kScreenshot ||
         restriction == Restriction::kScreenShare);

  const std::set<RuleId> source_rules_ids = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  return GetMaxJoinRestrictionLevel(restriction, source_rules_ids);
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedDestination(
    const GURL& source,
    const GURL& destination,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  // Allow copy/paste within the same document.
  if (url::Origin::Create(source).IsSameOriginWith(
          url::Origin::Create(destination)))
    return Level::kAllow;

  const std::set<RuleId> source_rules_ids = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const std::set<RuleId> destination_rules_ids = MatchUrlAndGetRulesMapping(
      destination, dst_url_matcher_.get(), dst_url_rules_mapping_);

  return GetMaxJoinRestrictionLevel(restriction, source_rules_ids,
                                    destination_rules_ids);
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedComponent(
    const GURL& source,
    const Component& destination,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard);

  const std::set<RuleId> source_rules_ids = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  auto it = components_rules_.find(destination);
  if (it == components_rules_.end())
    return Level::kAllow;

  const std::set<RuleId>& components_rules_ids = it->second;

  return GetMaxJoinRestrictionLevel(restriction, source_rules_ids,
                                    components_rules_ids);
}

DlpRulesManagerImpl::DlpRulesManagerImpl(PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManagerImpl::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();
}

void DlpRulesManagerImpl::OnPolicyUpdate() {
  components_rules_.clear();
  restrictions_map_.clear();
  src_url_rules_mapping_.clear();
  dst_url_rules_mapping_.clear();
  src_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  dst_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();

  if (!base::FeatureList::IsEnabled(features::kDataLeakPreventionPolicy)) {
    return;
  }

  const base::ListValue* rules_list =
      g_browser_process->local_state()->GetList(policy_prefs::kDlpRulesList);

  DlpBooleanHistogram(dlp::kDlpPolicyPresentUMA,
                      rules_list && !rules_list->empty());
  if (!rules_list || rules_list->empty()) {
    DataTransferDlpController::DeleteInstance();
    return;
  }

  RuleId rules_counter = 0;
  UrlConditionId src_url_condition_id = 0;
  UrlConditionId dst_url_condition_id = 0;

  // Constructing request to send the policy to DLP Files daemon.
  ::dlp::SetDlpFilesPolicyRequest request_to_daemon;

  for (const base::Value& rule : *rules_list) {
    DCHECK(rule.is_dict());
    const auto* sources = rule.FindDictKey("sources");
    DCHECK(sources);
    const auto* sources_urls = sources->FindListKey("urls");
    DCHECK(sources_urls);  // This DCHECK should be removed when other types are
                           // supported as sources.

    UrlConditionId prev_src_url_condition_id = src_url_condition_id;
    url_util::AddFilters(src_url_matcher_.get(), /* allowed= */ true,
                         &src_url_condition_id,
                         &base::Value::AsListValue(*sources_urls));
    InsertUrlsRulesMapping(prev_src_url_condition_id + 1, src_url_condition_id,
                           rules_counter, src_url_rules_mapping_);

    const auto* destinations = rule.FindDictKey("destinations");
    const auto* destinations_urls =
        destinations ? destinations->FindListKey("urls") : nullptr;
    if (destinations_urls) {
      UrlConditionId prev_dst_url_condition_id = dst_url_condition_id;
      url_util::AddFilters(dst_url_matcher_.get(), /* allowed= */ true,
                           &dst_url_condition_id,
                           &base::Value::AsListValue(*destinations_urls));
      InsertUrlsRulesMapping(prev_dst_url_condition_id + 1,
                             dst_url_condition_id, rules_counter,
                             dst_url_rules_mapping_);
    }
    const auto* destinations_components =
        destinations ? destinations->FindListKey("components") : nullptr;
    if (destinations_components) {
      for (const auto& component : destinations_components->GetList()) {
        DCHECK(component.is_string());
        components_rules_[GetComponentMapping(component.GetString())].insert(
            rules_counter);
      }
    }

    const auto* restrictions = rule.FindListKey("restrictions");
    DCHECK(restrictions);
    for (const auto& restriction : restrictions->GetList()) {
      const auto* rule_class_str = restriction.FindStringKey("class");
      DCHECK(rule_class_str);
      const auto* rule_level_str = restriction.FindStringKey("level");
      DCHECK(rule_level_str);

      const Restriction rule_restriction = GetClassMapping(*rule_class_str);
      if (rule_restriction == Restriction::kUnknownRestriction)
        continue;

      Level rule_level = GetLevelMapping(*rule_level_str);
      if (rule_level == Level::kNotSet)
        continue;

      // TODO(crbug.com/1172959): Implement Warn level for Files.
      if (rule_restriction == Restriction::kFiles && destinations_urls &&
          !destinations_urls->GetList().empty() && rule_level != Level::kWarn) {
        ::dlp::DlpFilesRule files_rule;
        for (const auto& url : sources_urls->GetList()) {
          DCHECK(url.is_string());
          files_rule.add_source_urls(url.GetString());
        }
        for (const auto& url : destinations_urls->GetList()) {
          DCHECK(url.is_string());
          files_rule.add_destination_urls(url.GetString());
        }
        files_rule.set_level(GetLevelProtoEnum(rule_level));
        request_to_daemon.mutable_rules()->Add(std::move(files_rule));
      }

      DlpRestrictionConfiguredHistogram(rule_restriction);
      restrictions_map_[rule_restriction].emplace(rules_counter, rule_level);
    }
    ++rules_counter;
  }

  if (base::Contains(restrictions_map_, Restriction::kClipboard)) {
    DataTransferDlpController::Init(*this);
  } else {
    DataTransferDlpController::DeleteInstance();
  }

  // TODO(crbug.com/1174501) Shutdown the daemon when restrictions are empty.
  if (request_to_daemon.rules_size() > 0) {
    DlpBooleanHistogram(dlp::kFilesDaemonStartedUMA, true);
    chromeos::DlpClient::Get()->SetDlpFilesPolicy(
        request_to_daemon, base::BindOnce(&OnSetDlpFilesPolicy));
  }
}

DlpRulesManager::Level DlpRulesManagerImpl::GetMaxJoinRestrictionLevel(
    const Restriction restriction,
    const std::set<RuleId>& selected_rules) const {
  auto restriction_it = restrictions_map_.find(restriction);
  if (restriction_it == restrictions_map_.end())
    return Level::kAllow;

  const std::map<RuleId, Level>& restriction_levels = restriction_it->second;

  Level max_level = Level::kNotSet;
  for (RuleId rule_id : selected_rules) {
    const auto& restriction_level_it = restriction_levels.find(rule_id);
    if (restriction_level_it == restriction_levels.end()) {
      continue;
    }
    max_level = GetMaxLevel(max_level, restriction_level_it->second);
  }

  if (max_level == Level::kNotSet)
    return Level::kAllow;
  return max_level;
}

DlpRulesManager::Level DlpRulesManagerImpl::GetMaxJoinRestrictionLevel(
    const Restriction restriction,
    const std::set<RuleId>& source_rules,
    const std::set<RuleId>& destination_rules) const {
  std::set<UrlConditionId> intersection;
  std::set_intersection(source_rules.begin(), source_rules.end(),
                        destination_rules.begin(), destination_rules.end(),
                        std::inserter(intersection, intersection.begin()));
  return GetMaxJoinRestrictionLevel(restriction, intersection);
}

}  // namespace policy
