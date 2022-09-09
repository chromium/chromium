// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_rules_config.h"

#include "ash/constants/ash_features.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace ash {
namespace input_method {
namespace {

// String parameter containing the JSON-encoded rules dictionary.
const char kJsonRulesDictKey[] = "json_rules";

constexpr base::FeatureParam<std::string> kFieldTrialParams{
    &ash::features::kImeRuleConfig, kJsonRulesDictKey, ""};

// Array of objects, containing the list of rules.
const char kConfigRulesKey[] = "rules";

// Array of strings, containing the list of items the rule will use.
const char kConfigRuleItemsKey[] = "items";

// String, rule name of autocorrect domain denylist, containing the list of
// globally denylisted domains for auto correct..
const char kAutocorrectDomainDenylistKey[] = "ac-domain-denylist";

}  // namespace

ImeRulesConfig::ImeRulesConfig() {
  InitFromTrialParams();
}

ImeRulesConfig::~ImeRulesConfig() = default;

void ImeRulesConfig::InitFromTrialParams() {
  std::string params = kFieldTrialParams.Get();
  if (params.empty()) {
    VLOG(2) << "Field trial parameter not set";
    return;
  }
  auto dict = base::JSONReader::ReadAndReturnValueWithError(params);
  if (!dict.has_value() || !dict->is_dict()) {
    VLOG(1) << "Failed to parse field trial params as JSON object: " << params;
    if (VLOG_IS_ON(1)) {
      if (dict.has_value())
        VLOG(1) << "Expecting a dictionary";
      else
        VLOG(1) << dict.error().message << ", line: " << dict.error().line
                << ", col: " << dict.error().column;
    }
    return;
  }

  // Read mandatory list of rules.
  auto* rules = dict->FindDictKey(kConfigRulesKey);
  if (rules == nullptr || !rules->is_dict()) {
    VLOG(1) << "Field trial params did not contain rules";
    return;
  }

  // Read optional rule for auto correct deny list.
  auto* ac_domain_denylist = rules->FindDictKey(kAutocorrectDomainDenylistKey);
  if (ac_domain_denylist == nullptr || !ac_domain_denylist->is_dict()) {
    VLOG(1) << "Rules from config did not contain "
            << kAutocorrectDomainDenylistKey;
  } else {
    // Read optional list of auto correct denylisted domains.
    auto* ac_domains_items =
        ac_domain_denylist->FindListKey(kConfigRuleItemsKey);
    if (ac_domains_items != nullptr) {
      for (const auto& domain : ac_domains_items->GetListDeprecated()) {
        if (domain.is_string()) {
          rule_auto_correct_domain_denylist_.push_back(*domain.GetIfString());
        }
      }
    }
  }
}

bool ImeRulesConfig::IsAutoCorrectDisabled(
    const TextFieldContextualInfo& info) {
  // Check the default domain denylist rules.
  for (const auto& domain : default_auto_correct_domain_denylist_) {
    if (info.tab_url.DomainIs(domain)) {
      return true;
    }
  }
  // Check the rule domain denylist rules.
  for (const auto& domain : rule_auto_correct_domain_denylist_) {
    if (info.tab_url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

// static
ImeRulesConfig* ImeRulesConfig::GetInstance() {
  return base::Singleton<ImeRulesConfig>::get();
}

}  // namespace input_method
}  // namespace ash
