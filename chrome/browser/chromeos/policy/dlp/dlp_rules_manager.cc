// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

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
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace dlp {

constexpr char kClipboardRestriction[] = "CLIPBOARD";
constexpr char kScreenshotRestriction[] = "SCREENSHOT";
constexpr char kPrintingRestriction[] = "PRINTING";
constexpr char kPrivacyScreenRestriction[] = "PRIVACY_SCREEN";
constexpr char kScreenShareRestriction[] = "SCREEN_SHARE";

constexpr char kArc[] = "ARC";
constexpr char kCrostini[] = "CROSTINI";
constexpr char kPluginVm[] = "PLUGIN_VM";

constexpr char kAllowLevel[] = "ALLOW";
constexpr char kBlockLevel[] = "BLOCK";

}  // namespace dlp

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
            DlpRulesManager::Restriction::kScreenShare}});

  auto* it = kRestrictionsMap.find(restriction);
  return (it == kRestrictionsMap.end())
             ? DlpRulesManager::Restriction::kUnknownRestriction
             : it->second;
}

DlpRulesManager::Level GetLevelMapping(const std::string& level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Level>(
          {{dlp::kAllowLevel, DlpRulesManager::Level::kAllow},
           {dlp::kBlockLevel, DlpRulesManager::Level::kBlock}});
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
           {DlpRulesManager::Level::kBlock, 1},
           {DlpRulesManager::Level::kAllow, 2}});
  return kPrioritiesMap.at(level);
}

DlpRulesManager::Level GetMaxLevel(const DlpRulesManager::Level& level_1,
                                   const DlpRulesManager::Level& level_2) {
  return GetPriorityMapping(level_1) > GetPriorityMapping(level_2) ? level_1
                                                                   : level_2;
}

DlpRulesManager::Level GetMinLevel(const DlpRulesManager::Level& level_1,
                                   const DlpRulesManager::Level& level_2) {
  return GetPriorityMapping(level_1) < GetPriorityMapping(level_2) ? level_1
                                                                   : level_2;
}

// Inserts a mapping between URLs conditions IDs range to `rule_id` in `map`.
void InsertUrlsRulesMapping(
    DlpRulesManager::UrlConditionId url_condition_id_start,
    DlpRulesManager::UrlConditionId url_condition_id_end,
    DlpRulesManager::RuleId rule_id,
    std::map<DlpRulesManager::UrlConditionId, DlpRulesManager::RuleId>& map) {
  for (auto url_condition_id = url_condition_id_start;
       url_condition_id <= url_condition_id_end; ++url_condition_id) {
    map[url_condition_id] = rule_id;
  }
}

// Matches `url` against `url_matcher` patterns and returns the rules IDs
// configured with the matched patterns.
std::set<DlpRulesManager::RuleId> MatchUrlAndGetRulesMapping(
    const GURL& url,
    const url_matcher::URLMatcher* url_matcher,
    const std::map<DlpRulesManager::UrlConditionId, DlpRulesManager::RuleId>&
        rules_map) {
  DCHECK(url_matcher);
  const std::set<DlpRulesManager::UrlConditionId> url_conditions_ids =
      url_matcher->MatchURL(url);

  std::set<DlpRulesManager::RuleId> rule_ids;
  for (const auto& id : url_conditions_ids) {
    rule_ids.insert(rules_map.at(id));
  }
  return rule_ids;
}

// A singleton instance of DlpRulesManager for testing.
static DlpRulesManager* g_dlp_rules_manager_for_testing = nullptr;

}  // namespace

// static
void DlpRulesManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(policy_prefs::kDlpRulesList);
}

DlpRulesManager::Level DlpRulesManager::IsRestricted(
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

DlpRulesManager::Level DlpRulesManager::IsRestrictedDestination(
    const GURL& source,
    const GURL& destination,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard);

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

DlpRulesManager::Level DlpRulesManager::IsRestrictedComponent(
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

DlpRulesManager::DlpRulesManager(PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManager::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();
}

DlpRulesManager::~DlpRulesManager() = default;

void DlpRulesManager::OnPolicyUpdate() {
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

  if (!rules_list) {
    DataTransferDlpController::DeleteInstance();
    return;
  }

  RuleId rules_counter = 0;
  UrlConditionId src_url_condition_id = 0;
  UrlConditionId dst_url_condition_id = 0;

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
    if (destinations) {
      const auto* destinations_urls = destinations->FindListKey("urls");
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
          destinations->FindListKey("components");
      if (destinations_components) {
        for (const auto& component : destinations_components->GetList()) {
          DCHECK(component.is_string());
          components_rules_[GetComponentMapping(component.GetString())].insert(
              rules_counter);
        }
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

      restrictions_map_[rule_restriction].emplace(rules_counter, rule_level);
    }
    ++rules_counter;
  }

  if (base::Contains(restrictions_map_, Restriction::kClipboard)) {
    DataTransferDlpController::Init();
  } else {
    DataTransferDlpController::DeleteInstance();
  }
}

DlpRulesManager::Level DlpRulesManager::GetMaxJoinRestrictionLevel(
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

DlpRulesManager::Level DlpRulesManager::GetMaxJoinRestrictionLevel(
    const Restriction restriction,
    const std::set<RuleId>& source_rules,
    const std::set<RuleId>& destination_rules) const {
  std::set<UrlConditionId> intersection;
  std::set_intersection(source_rules.begin(), source_rules.end(),
                        destination_rules.begin(), destination_rules.end(),
                        std::inserter(intersection, intersection.begin()));
  return GetMaxJoinRestrictionLevel(restriction, intersection);
}

// static
DlpRulesManagerFactory* DlpRulesManagerFactory::GetInstance() {
  static base::NoDestructor<DlpRulesManagerFactory> factory;
  return factory.get();
}

// static
DlpRulesManager* DlpRulesManagerFactory::GetForPrimaryProfile() {
  if (g_dlp_rules_manager_for_testing)
    return g_dlp_rules_manager_for_testing;

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile)
    return nullptr;
  return static_cast<DlpRulesManager*>(
      DlpRulesManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/false));
}

// static
void DlpRulesManagerFactory::OverrideManagerForTesting(
    DlpRulesManager* manager) {
  g_dlp_rules_manager_for_testing = manager;
}

DlpRulesManagerFactory::DlpRulesManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DlpRulesManager",
          BrowserContextDependencyManager::GetInstance()) {}

bool DlpRulesManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* DlpRulesManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // UserManager might be not available in tests.
  if (!user_manager::UserManager::IsInitialized() || !profile ||
      !chromeos::ProfileHelper::IsPrimaryProfile(profile) ||
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  PrefService* local_state = g_browser_process->local_state();
  // Might be not available in tests.
  if (!local_state)
    return nullptr;

  return new DlpRulesManager(local_state);
}

}  // namespace policy
