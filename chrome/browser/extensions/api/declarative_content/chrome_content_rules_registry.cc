// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension_id.h"

namespace extensions {

//
// EvaluationScope
//

// Used to coalesce multiple requests for evaluation into a zero or one actual
// evaluations (depending on the EvaluationDisposition).  This is required for
// correctness when multiple trackers respond to the same event. Otherwise,
// executing the request from the first tracker will be done before the tracked
// state has been updated for the other trackers.
class ChromeContentRulesRegistry::EvaluationScope {
 public:
  // Default disposition is PERFORM_EVALUATION.
  explicit EvaluationScope(ChromeContentRulesRegistry* registry);
  EvaluationScope(ChromeContentRulesRegistry* registry,
                  EvaluationDisposition disposition);
  ~EvaluationScope();

 private:
  ChromeContentRulesRegistry* const registry_;
  const EvaluationDisposition previous_disposition_;

  DISALLOW_COPY_AND_ASSIGN(EvaluationScope);
};

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry)
    : EvaluationScope(registry, DEFER_REQUESTS) {}

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry,
    EvaluationDisposition disposition)
    : registry_(registry),
      previous_disposition_(registry_->evaluation_disposition_) {
  DCHECK_NE(EVALUATE_REQUESTS, disposition);
  registry_->evaluation_disposition_ = disposition;
}

ChromeContentRulesRegistry::EvaluationScope::~EvaluationScope() {
  registry_->evaluation_disposition_ = previous_disposition_;
  if (registry_->evaluation_disposition_ == EVALUATE_REQUESTS) {
    for (content::WebContents* tab : registry_->evaluation_pending_)
      registry_->EvaluateConditionsForTab(tab);
    registry_->evaluation_pending_.clear();
  }
}

//
// ChromeContentRulesRegistry
//

ChromeContentRulesRegistry::ChromeContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate,
    const PredicateEvaluatorsFactory& evaluators_factory)
    : ContentRulesRegistry(browser_context,
                           declarative_content_constants::kOnPageChanged,
                           content::BrowserThread::UI,
                           cache_delegate,
                           RulesRegistryService::kDefaultRulesRegistryID),
      evaluators_(evaluators_factory.Run(this)),
      evaluation_disposition_(EVALUATE_REQUESTS) {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void ChromeContentRulesRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);

  // Note that neither non-tab WebContents nor tabs from other browser
  // contexts will be in the map.
  active_rules_.erase(content::Source<content::WebContents>(source).ptr());
}

void ChromeContentRulesRegistry::RequestEvaluation(
    content::WebContents* contents) {
  switch (evaluation_disposition_) {
    case EVALUATE_REQUESTS:
      EvaluateConditionsForTab(contents);
      break;
    case DEFER_REQUESTS:
      evaluation_pending_.insert(contents);
      break;
    case IGNORE_REQUESTS:
      break;
  }
}

bool ChromeContentRulesRegistry::ShouldManageConditionsForBrowserContext(
    content::BrowserContext* context) {
  return ManagingRulesForBrowserContext(context);
}

void ChromeContentRulesRegistry::MonitorWebContentsForRuleEvaluation(
    content::WebContents* contents) {
  // We rely on active_rules_ to have a key-value pair for |contents| to know
  // which WebContents we are working with.
  active_rules_[contents] = std::set<const ContentRule*>();

  EvaluationScope evaluation_scope(this);
  for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
       evaluators_)
    evaluator->TrackForWebContents(contents);
}

void ChromeContentRulesRegistry::DidFinishNavigation(
    content::WebContents* contents,
    content::NavigationHandle* navigation_handle) {
  if (base::Contains(active_rules_, contents)) {
    EvaluationScope evaluation_scope(this);
    for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
         evaluators_)
      evaluator->OnWebContentsNavigation(contents, navigation_handle);
  }
}

ChromeContentRulesRegistry::ContentRule::ContentRule(
    const Extension* extension,
    std::vector<std::unique_ptr<const ContentCondition>> conditions,
    std::vector<std::unique_ptr<const ContentAction>> actions,
    int priority)
    : extension(extension),
      conditions(std::move(conditions)),
      actions(std::move(actions)),
      priority(priority) {}

ChromeContentRulesRegistry::ContentRule::~ContentRule() {}

std::unique_ptr<const ChromeContentRulesRegistry::ContentRule>
ChromeContentRulesRegistry::CreateRule(
    const Extension* extension,
    const std::map<std::string, ContentPredicateFactory*>& predicate_factories,
    const api::events::Rule& api_rule,
    std::string* error) {
  std::vector<std::unique_ptr<const ContentCondition>> conditions;
  for (const std::unique_ptr<base::Value>& value : api_rule.conditions) {
    conditions.push_back(
        CreateContentCondition(extension, predicate_factories, *value, error));
    if (!error->empty())
      return std::unique_ptr<ContentRule>();
  }

  std::vector<std::unique_ptr<const ContentAction>> actions;
  for (const std::unique_ptr<base::Value>& value : api_rule.actions) {
    actions.push_back(ContentAction::Create(browser_context(), extension,
                                            *value, error));
    if (!error->empty())
      return std::unique_ptr<ContentRule>();
  }

  // Note: |api_rule| may contain tags, but these are ignored.

  return std::make_unique<ContentRule>(extension, std::move(conditions),
                                       std::move(actions), *api_rule.priority);
}

bool ChromeContentRulesRegistry::ManagingRulesForBrowserContext(
    content::BrowserContext* context) {
  // Manage both the normal context and incognito contexts associated with it.
  return Profile::FromBrowserContext(context)->GetOriginalProfile() ==
      Profile::FromBrowserContext(browser_context());
}

// static
bool ChromeContentRulesRegistry::EvaluateConditionForTab(
    const ContentCondition* condition,
    content::WebContents* tab) {
  for (const std::unique_ptr<const ContentPredicate>& predicate :
       condition->predicates) {
    if (predicate && !predicate->IsIgnored() &&
        !predicate->GetEvaluator()->EvaluatePredicate(predicate.get(), tab)) {
      return false;
    }
  }

  return true;
}

std::set<const ChromeContentRulesRegistry::ContentRule*>
ChromeContentRulesRegistry::GetMatchingRules(content::WebContents* tab) const {
  const bool is_incognito_tab = tab->GetBrowserContext()->IsOffTheRecord();
  std::set<const ContentRule*> matching_rules;
  for (const RulesMap::value_type& rule_id_rule_pair : content_rules_) {
    const ContentRule* rule = rule_id_rule_pair.second.get();
    if (is_incognito_tab &&
        !ShouldEvaluateExtensionRulesForIncognitoRenderer(rule->extension))
      continue;

    for (const std::unique_ptr<const ContentCondition>& condition :
         rule->conditions) {
      if (EvaluateConditionForTab(condition.get(), tab))
        matching_rules.insert(rule);
    }
  }
  return matching_rules;
}

std::string ChromeContentRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<const api::events::Rule*>& api_rules) {
  EvaluationScope evaluation_scope(this);
  const Extension* extension = ExtensionRegistry::Get(browser_context())
      ->GetInstalledExtension(extension_id);
  DCHECK(extension);

  std::string error;
  RulesMap new_rules;
  std::map<ContentPredicateEvaluator*,
           std::map<const void*, std::vector<const ContentPredicate*>>>
      new_predicates;

  std::map<std::string, ContentPredicateFactory*> predicate_factories;
  for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
       evaluators_) {
    predicate_factories[evaluator->GetPredicateApiAttributeName()] =
        evaluator.get();
  }

  for (auto* api_rule : api_rules) {
    ExtensionIdRuleIdPair rule_id(extension_id, *api_rule->id);
    DCHECK(content_rules_.find(rule_id) == content_rules_.end());

    std::unique_ptr<const ContentRule> rule(
        CreateRule(extension, predicate_factories, *api_rule, &error));
    if (!error.empty()) {
      // Notify evaluators that none of the created predicates will be tracked
      // after all.
      for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
           evaluators_) {
        if (!new_predicates[evaluator.get()].empty()) {
          evaluator->TrackPredicates(
              std::map<const void*, std::vector<const ContentPredicate*>>());
        }
      }

      return error;
    }
    DCHECK(rule);

    // Group predicates by evaluator and rule, so we can later notify the
    // evaluators that they have new predicates to manage.
    for (const std::unique_ptr<const ContentCondition>& condition :
         rule->conditions) {
      for (const std::unique_ptr<const ContentPredicate>& predicate :
           condition->predicates) {
        if (predicate.get()) {
          new_predicates[predicate->GetEvaluator()][rule.get()].push_back(
              predicate.get());
        }
      }
    }

    new_rules[rule_id] = std::move(rule);
  }

  // Notify the evaluators about their new predicates.
  for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
       evaluators_)
    evaluator->TrackPredicates(new_predicates[evaluator.get()]);

  // Wohoo, everything worked fine.
  content_rules_.insert(std::make_move_iterator(new_rules.begin()),
                        std::make_move_iterator(new_rules.end()));

  // Request evaluation for all WebContents, under the assumption that a
  // non-empty condition has been added.
  for (const auto& web_contents_rules_pair : active_rules_)
    RequestEvaluation(web_contents_rules_pair.first);

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // Ignore evaluation requests in this function because it reverts actions on
  // any active rules itself. Otherwise, we run the risk of reverting the same
  // rule multiple times.
  EvaluationScope evaluation_scope(this, IGNORE_REQUESTS);

  std::vector<RulesMap::iterator> rules_to_erase;
  std::vector<const void*> predicate_groups_to_stop_tracking;
  for (const std::string& id : rule_identifiers) {
    // Skip unknown rules.
    auto content_rules_entry =
        content_rules_.find(std::make_pair(extension_id, id));
    if (content_rules_entry == content_rules_.end())
      continue;

    const ContentRule* rule = content_rules_entry->second.get();

    // Remove the ContentRule from active_rules_.
    for (auto& tab_rules_pair : active_rules_) {
      if (base::Contains(tab_rules_pair.second, rule)) {
        ContentAction::ApplyInfo apply_info =
            {rule->extension, browser_context(), tab_rules_pair.first,
             rule->priority};
        for (const auto& action : rule->actions)
          action->Revert(apply_info);
        tab_rules_pair.second.erase(rule);
      }
    }

    rules_to_erase.push_back(content_rules_entry);
    predicate_groups_to_stop_tracking.push_back(rule);
  }

  // Notify the evaluators to stop tracking the predicates that will be removed.
  for (const std::unique_ptr<ContentPredicateEvaluator>& evaluator :
       evaluators_)
    evaluator->StopTrackingPredicates(predicate_groups_to_stop_tracking);

  // Remove the rules.
  for (auto it : rules_to_erase)
    content_rules_.erase(it);

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  // Search all identifiers of rules that belong to extension |extension_id|.
  std::vector<std::string> rule_identifiers;
  for (const RulesMap::value_type& id_rule_pair : content_rules_) {
    const ExtensionIdRuleIdPair& extension_id_rule_id_pair = id_rule_pair.first;
    if (extension_id_rule_id_pair.first == extension_id)
      rule_identifiers.push_back(extension_id_rule_id_pair.second);
  }

  return RemoveRulesImpl(extension_id, rule_identifiers);
}

void ChromeContentRulesRegistry::EvaluateConditionsForTab(
    content::WebContents* tab) {
  std::set<const ContentRule*> matching_rules = GetMatchingRules(tab);
  if (matching_rules.empty() && !base::Contains(active_rules_, tab))
    return;

  std::set<const ContentRule*>& prev_matching_rules = active_rules_[tab];
  for (const ContentRule* rule : matching_rules) {
    ContentAction::ApplyInfo apply_info =
        {rule->extension, browser_context(), tab, rule->priority};
    if (!base::Contains(prev_matching_rules, rule)) {
      for (const std::unique_ptr<const ContentAction>& action : rule->actions)
        action->Apply(apply_info);
    } else {
      for (const std::unique_ptr<const ContentAction>& action : rule->actions)
        action->Reapply(apply_info);
    }
  }
  for (const ContentRule* rule : prev_matching_rules) {
    if (!base::Contains(matching_rules, rule)) {
      ContentAction::ApplyInfo apply_info =
          {rule->extension, browser_context(), tab, rule->priority};
      for (const std::unique_ptr<const ContentAction>& action : rule->actions)
        action->Revert(apply_info);
    }
  }

  if (matching_rules.empty())
    active_rules_[tab].clear();
  else
    swap(matching_rules, prev_matching_rules);
}

bool
ChromeContentRulesRegistry::ShouldEvaluateExtensionRulesForIncognitoRenderer(
    const Extension* extension) const {
  if (!util::IsIncognitoEnabled(extension->id(), browser_context()))
    return false;

  // Split-mode incognito extensions register their rules with separate
  // RulesRegistries per Original/OffTheRecord browser contexts, whereas
  // spanning-mode extensions share the Original browser context.
  if (util::CanCrossIncognito(extension, browser_context())) {
    // The extension uses spanning mode incognito. No rules should have been
    // registered for the extension in the OffTheRecord registry so
    // execution for that registry should never reach this point.
    CHECK(!browser_context()->IsOffTheRecord());
  } else {
    // The extension uses split mode incognito. Both the Original and
    // OffTheRecord registries may have (separate) rules for this extension.
    // Since we're looking at an incognito renderer, so only the OffTheRecord
    // registry should process its rules.
    if (!browser_context()->IsOffTheRecord())
      return false;
  }

  return true;
}

size_t ChromeContentRulesRegistry::GetActiveRulesCountForTesting() {
  size_t count = 0;
  for (const auto& web_contents_rules_pair : active_rules_)
    count += web_contents_rules_pair.second.size();
  return count;
}

ChromeContentRulesRegistry::~ChromeContentRulesRegistry() {
}

}  // namespace extensions
