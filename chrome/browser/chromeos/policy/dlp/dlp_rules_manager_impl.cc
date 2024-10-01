// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_util.h"
#include "url/origin.h"

namespace policy {

namespace {

using RuleId = DlpRulesManagerImpl::RuleId;

using UrlConditionId = DlpRulesManagerImpl::UrlConditionId;

using RulesConditionsMap = std::map<RuleId, UrlConditionId>;

constexpr char kDrivePattern[] = "drive.google.com";
constexpr char kOneDrivePattern[] = "onedrive.live.com";

// Creates a condition set for the given `url`.
scoped_refptr<url_matcher::URLMatcherConditionSet> CreateConditionSet(
    url_matcher::URLMatcher* matcher,
    UrlConditionId condition_id,
    const std::string& url) {
  CHECK(matcher);

  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  bool match_subdomains = true;

  if (!url_matcher::util::FilterToComponents(
          url, &scheme, &host, &match_subdomains, &port, &path, &query)) {
    LOG(ERROR) << "Invalid pattern " << url;
    return nullptr;
  }
  return url_matcher::util::CreateConditionSet(matcher, condition_id, scheme,
                                               host, match_subdomains, port,
                                               path, query, /*allow=*/true);
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
  for (const auto& list_entry : *urls) {
    std::string url = list_entry.GetString();

    ++condition_id;
    auto condition_set = CreateConditionSet(matcher, condition_id, url);
    if (!condition_set) {
      continue;
    }
    conditions.push_back(std::move(condition_set));
    map[condition_id] = rule_id;
    patterns_mapping[condition_id] = url;
  }
}

// Returns the URLs associated with the given component. An empty vector is
// returned if there are none.
std::vector<std::string> GetAssociatedUrlsConditions(
    data_controls::Component component) {
  switch (component) {
    case data_controls::Component::kDrive:
      return {kDrivePattern};
    case data_controls::Component::kOneDrive:
      return {kOneDrivePattern};
    case data_controls::Component::kUnknownComponent:
    case data_controls::Component::kArc:
    case data_controls::Component::kCrostini:
    case data_controls::Component::kPluginVm:
    case data_controls::Component::kUsb:
      return {};
  }
}

// Add URL conditions associated with the given `component`.
void AddAssociatedUrlConditions(
    data_controls::Component component,
    url_matcher::URLMatcher* matcher,
    UrlConditionId& condition_id,
    url_matcher::URLMatcherConditionSet::Vector& conditions,
    std::map<UrlConditionId, std::string>& patterns_mapping,
    RuleId rule_id,
    std::map<UrlConditionId, RuleId>& map) {
  base::Value::List destinations_urls;

  for (const auto& url : GetAssociatedUrlsConditions(component)) {
    destinations_urls.Append(url);
  }

  if (!destinations_urls.empty()) {
    AddUrlConditions(matcher, condition_id, &destinations_urls, conditions,
                     patterns_mapping, rule_id, map);
  }
}

void OnSetDlpFilesPolicy(const ::dlp::SetDlpFilesPolicyResponse response) {
  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kErrorsFilesPolicySetup,
      response.has_error_message());
  if (response.has_error_message()) {
    DlpScopedFileAccessDelegate::DeleteInstance();
    LOG(ERROR) << "Failed to set DLP Files policy and start DLP daemon, error: "
               << response.error_message();
    return;
  }
  DCHECK(chromeos::DlpClient::Get()->IsAlive());
  DlpScopedFileAccessDelegate::Initialize(
      base::BindRepeating(chromeos::DlpClient::Get));
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

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedComponent(
    const GURL& source,
    const data_controls::Component& destination,
    Restriction restriction,
    std::string* out_source_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  if (destination == data_controls::Component::kUnknownComponent) {
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

DlpRulesManager::AggregatedComponents
DlpRulesManagerImpl::GetAggregatedComponents(const GURL& source,
                                             Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  std::map<Level, std::set<data_controls::Component>> result;
  for (data_controls::Component component : data_controls::kAllComponents) {
    std::string out_source_pattern;
    Level level = IsRestrictedComponent(source, component, restriction,
                                        &out_source_pattern, nullptr);
    result[level].insert(component);
  }

  return result;
}

DlpRulesManagerImpl::DlpRulesManagerImpl(PrefService* local_state,
                                         Profile* profile)
    : DlpRulesManager(profile) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManagerImpl::OnDataLeakPreventionRulesUpdate,
                          base::Unretained(this)));
  OnDataLeakPreventionRulesUpdate();

  if (IsReportingEnabled())
    reporting_manager_ = std::make_unique<data_controls::DlpReportingManager>();

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

data_controls::DlpReportingManager* DlpRulesManagerImpl::GetReportingManager()
    const {
  return reporting_manager_.get();
}

DlpFilesController* DlpRulesManagerImpl::GetDlpFilesController() const {
  return files_controller_.get();
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
  OnDataLeakPreventionRulesUpdate();
}

void DlpRulesManagerImpl::Shutdown() {
  // There are FilesController implementations such as DlpFilesControllerAsh
  // that are using the Profile to do some cleanup (e.g., stop observing the
  // VolumeManager). This cleanup must be done when the KeyedService::Shutdown
  // method is called.
  files_controller_.reset();
}

void DlpRulesManagerImpl::OnDataLeakPreventionRulesUpdate() {
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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  files_controller_ = nullptr;
#endif

  if (!base::FeatureList::IsEnabled(features::kDataLeakPreventionPolicy)) {
    return;
  }

  const base::Value::List& rules_list =
      g_browser_process->local_state()->GetList(policy_prefs::kDlpRulesList);

  data_controls::DlpBooleanHistogram(data_controls::dlp::kDlpPolicyPresentUMA,
                                     !rules_list.empty());
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
        data_controls::Component component_mapping =
            data_controls::GetComponentMapping(component.GetString());
        components_rules_[component_mapping].insert(rules_counter);
        AddAssociatedUrlConditions(component_mapping, dst_url_matcher_.get(),
                                   dst_url_condition_id, dst_conditions_,
                                   dst_patterns_mapping_, rules_counter,
                                   dst_url_rules_mapping_);
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

      const Restriction rule_restriction =
          data_controls::Rule::StringToRestriction(*rule_class_str);
      if (rule_restriction == Restriction::kUnknownRestriction)
        continue;

      Level rule_level = data_controls::Rule::StringToLevel(*rule_level_str);
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
                data_controls::GetComponentProtoMapping(component.GetString()));
            for (const auto& url :
                 GetAssociatedUrlsConditions(data_controls::GetComponentMapping(
                     component.GetString()))) {
              files_rule.add_destination_urls(url);
            }
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
      || (base::FeatureList::IsEnabled(
              features::kDataLeakPreventionFilesRestriction) &&
          request_to_daemon.rules_size() > 0)
  ) {
    DataTransferDlpController::Init(*this);
  } else {
    DataTransferDlpController::DeleteInstance();
  }

  if (base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    if (request_to_daemon.rules_size() > 0) {
      // Start and/or activate the daemon.
      data_controls::DlpBooleanHistogram(
          data_controls::dlp::kFilesDaemonStartedUMA, true);
      chromeos::DlpClient::Get()->SetDlpFilesPolicy(
          request_to_daemon, base::BindOnce(&OnSetDlpFilesPolicy));
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (!files_controller_) {
        files_controller_ =
            std::make_unique<DlpFilesControllerAsh>(*this, profile_);
      }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      if (!files_controller_) {
        files_controller_ = std::make_unique<DlpFilesControllerLacros>(*this);
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
