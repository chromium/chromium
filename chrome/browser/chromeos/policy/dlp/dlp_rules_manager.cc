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
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace dlp {

const char kClipboardRestriction[] = "CLIPBOARD";
const char kScreenshotRestriction[] = "SCREENSHOT";
const char kPrintingRestriction[] = "PRINTING";
const char kPrivacyScreenRestriction[] = "PRIVACY_SCREEN";

const char kArc[] = "ARC";
const char kCrostini[] = "CROSTINI";
const char kPluginVm[] = "PLUGIN_VM";

const char kAllowLevel[] = "ALLOW";
const char kBlockLevel[] = "BLOCK";

}  // namespace dlp

namespace {

DlpRulesManager::Restriction GetClassMapping(const std::string& restriction) {
  static const base::NoDestructor<
      std::map<std::string, DlpRulesManager::Restriction>>
      kRestrictionsMap(
          {{dlp::kClipboardRestriction,
            DlpRulesManager::Restriction::kClipboard},
           {dlp::kScreenshotRestriction,
            DlpRulesManager::Restriction::kScreenshot},
           {dlp::kPrintingRestriction, DlpRulesManager::Restriction::kPrinting},
           {dlp::kPrivacyScreenRestriction,
            DlpRulesManager::Restriction::kPrivacyScreen}});

  auto it = kRestrictionsMap->find(restriction);
  return (it == kRestrictionsMap->end())
             ? DlpRulesManager::Restriction::kUnknownRestriction
             : it->second;
}

DlpRulesManager::Level GetLevelMapping(const std::string& level) {
  static const base::NoDestructor<std::map<std::string, DlpRulesManager::Level>>
      kLevelsMap({{dlp::kAllowLevel, DlpRulesManager::Level::kAllow},
                  {dlp::kBlockLevel, DlpRulesManager::Level::kBlock}});
  auto it = kLevelsMap->find(level);
  return (it == kLevelsMap->end()) ? DlpRulesManager::Level::kNotSet
                                   : it->second;
}

DlpRulesManager::Component GetComponentMapping(const std::string& component) {
  static const base::NoDestructor<
      std::map<std::string, DlpRulesManager::Component>>
      kComponentsMap({{dlp::kArc, DlpRulesManager::Component::kArc},
                      {dlp::kCrostini, DlpRulesManager::Component::kCrostini},
                      {dlp::kPluginVm, DlpRulesManager::Component::kPluginVm}});

  auto it = kComponentsMap->find(component);
  return (it == kComponentsMap->end())
             ? DlpRulesManager::Component::kUnknownComponent
             : it->second;
}

uint8_t GetPriorityMapping(const DlpRulesManager::Level level) {
  static const base::NoDestructor<std::map<DlpRulesManager::Level, uint8_t>>
      kPrioritiesMap({{DlpRulesManager::Level::kNotSet, 0},
                      {DlpRulesManager::Level::kBlock, 1},
                      {DlpRulesManager::Level::kAllow, 2}});
  return kPrioritiesMap->at(level);
}

DlpRulesManager::Level GetMaxLevel(const DlpRulesManager::Level& level_1,
                                   const DlpRulesManager::Level& level_2) {
  return GetPriorityMapping(level_1) > GetPriorityMapping(level_2) ? level_1
                                                                   : level_2;
}

// A singleton instance of DlpRulesManager. Set from DlpRulesManager::Init().
static DlpRulesManager* g_dlp_rules_manager = nullptr;

}  // namespace

// static
void DlpRulesManager::Init() {
  if (!g_dlp_rules_manager)
    g_dlp_rules_manager = new DlpRulesManager();
}

// static
bool DlpRulesManager::IsInitialized() {
  return g_dlp_rules_manager != nullptr;
}

// static
DlpRulesManager* DlpRulesManager::Get() {
  DCHECK(g_dlp_rules_manager);
  return g_dlp_rules_manager;
}

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
         restriction == Restriction::kScreenshot);

  const std::set<url_matcher::URLMatcherConditionSet::ID> source_rules_ids =
      src_url_matcher_->MatchURL(source);

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

  std::set<url_matcher::URLMatcherConditionSet::ID> source_rules_ids =
      src_url_matcher_->MatchURL(source);

  std::set<url_matcher::URLMatcherConditionSet::ID> destination_rules_ids =
      dst_url_matcher_->MatchURL(destination);

  return GetMaxJoinRestrictionLevel(restriction, source_rules_ids,
                                    destination_rules_ids);
}

DlpRulesManager::Level DlpRulesManager::IsRestrictedComponent(
    const GURL& source,
    const Component& destination,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard);

  const std::set<url_matcher::URLMatcherConditionSet::ID> source_rules_ids =
      src_url_matcher_->MatchURL(source);

  auto it = components_rules_.find(destination);
  if (it == components_rules_.end())
    return Level::kAllow;

  const std::set<RuleId>& components_rules_ids = it->second;

  return GetMaxJoinRestrictionLevel(restriction, source_rules_ids,
                                    components_rules_ids);
}

DlpRulesManager::DlpRulesManager() {
  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManager::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();
}

DlpRulesManager::~DlpRulesManager() = default;

void DlpRulesManager::OnPolicyUpdate() {
  RuleId rules_counter = 0;

  const base::ListValue* rules_list =
      g_browser_process->local_state()->GetList(policy_prefs::kDlpRulesList);

  src_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  dst_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  components_rules_.clear();
  restrictions_map_.clear();

  if (!rules_list) {
    return;
  }

  for (const base::Value& rule : *rules_list) {
    DCHECK(rule.is_dict());
    const auto* sources = rule.FindDictKey("sources");
    DCHECK(sources);
    const auto* sources_urls = sources->FindListKey("urls");
    DCHECK(sources_urls);  // This DCHECK should be removed when other types are
                           // supported as sources.

    url_util::AddFilters(src_url_matcher_.get(), /* allowed= */ true,
                         &rules_counter,
                         &base::Value::AsListValue(*sources_urls));

    const auto* destinations = rule.FindDictKey("destinations");
    if (destinations) {
      const auto* destinations_urls = destinations->FindListKey("urls");
      if (destinations_urls) {
        int destination_rule_id = rules_counter - 1;
        url_util::AddFilters(dst_url_matcher_.get(), /* allowed= */ true,
                             &destination_rule_id,
                             &base::Value::AsListValue(*destinations_urls));
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
  std::set<url_matcher::URLMatcherConditionSet::ID> intersection;
  std::set_intersection(source_rules.begin(), source_rules.end(),
                        destination_rules.begin(), destination_rules.end(),
                        std::inserter(intersection, intersection.begin()));
  return GetMaxJoinRestrictionLevel(restriction, intersection);
}

}  // namespace policy
