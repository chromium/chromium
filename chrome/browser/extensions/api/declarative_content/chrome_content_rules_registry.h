// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class Extension;

// The ChromeContentRulesRegistry is responsible for managing
// the internal representation of rules for the Declarative Content API.
//
// Here is the high level overview of this functionality:
//
// api::events::Rule consists of conditions and actions, these are
// represented as a ContentRule with ContentConditions and ContentActions.
//
// A note on incognito support: separate instances of ChromeContentRulesRegistry
// are created for incognito and non-incognito contexts. The incognito instance,
// however, is only responsible for applying rules registered by the incognito
// side of split-mode extensions to incognito tabs. The non-incognito instance
// handles incognito tabs for spanning-mode extensions, plus all non-incognito
// tabs.
class ChromeContentRulesRegistry
    : public ContentRulesRegistry,
      public ContentPredicateEvaluator::Delegate {
 public:
  using PredicateEvaluatorsFactory = base::OnceCallback<
      std::vector<std::unique_ptr<ContentPredicateEvaluator>>(
          ContentPredicateEvaluator::Delegate*)>;

  // For testing, |cache_delegate| can be NULL. In that case it constructs the
  // registry with storage functionality suspended.
  ChromeContentRulesRegistry(content::BrowserContext* browser_context,
                             RulesCacheDelegate* cache_delegate,
                             PredicateEvaluatorsFactory evaluators_factory);

  ChromeContentRulesRegistry(const ChromeContentRulesRegistry&) = delete;
  ChromeContentRulesRegistry& operator=(const ChromeContentRulesRegistry&) =
      delete;

  // ContentRulesRegistry:
  void MonitorWebContentsForRuleEvaluation(
      content::WebContents* contents) override;
  void DidFinishNavigation(
      content::WebContents* tab,
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed(content::WebContents* contents) override;
  void OnWatchedPageChanged(
      content::WebContents* contents,
      const std::vector<std::string>& css_selectors) override;
  // RulesRegistry:
  std::string AddRulesImpl(
      const ExtensionId& extension_id,
      const std::vector<const api::events::Rule*>& rules) override;
  std::string RemoveRulesImpl(
      const ExtensionId& extension_id,
      const std::vector<std::string>& rule_identifiers) override;
  std::string RemoveAllRulesImpl(const ExtensionId& extension_id) override;

  // DeclarativeContentConditionTrackerDelegate:
  void RequestEvaluation(content::WebContents* contents) override;
  bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) override;

  // Returns the number of active rules.
  size_t GetActiveRulesCountForTesting();

 protected:
  ~ChromeContentRulesRegistry() override;

 private:
  // The internal declarative rule representation. Corresponds to a declarative
  // API rule: https://developer.chrome.com/extensions/events.html#declarative.
  struct ContentRule {
   public:
    ContentRule(const Extension* extension,
                std::vector<std::unique_ptr<const ContentCondition>> conditions,
                std::vector<std::unique_ptr<const ContentAction>> actions,
                int priority);

    ContentRule(const ContentRule&) = delete;
    ContentRule& operator=(const ContentRule&) = delete;

    ~ContentRule();

    raw_ptr<const Extension> extension;
    std::vector<std::unique_ptr<const ContentCondition>> conditions;
    std::vector<std::unique_ptr<const ContentAction>> actions;
    int priority;
  };

  // Specifies what to do with evaluation requests.
  // TODO(wittman): Try to eliminate the need for IGNORE after refactoring to
  // treat all condition evaluation consistently.
  enum EvaluationDisposition {
    EVALUATE_REQUESTS,  // Evaluate immediately.
    DEFER_REQUESTS,  // Defer for later evaluation.
    IGNORE_REQUESTS  // Ignore.
  };

  class EvaluationScope;

  // Creates a ContentRule for |extension| given a json definition.  The format
  // of each condition and action's json is up to the specific ContentCondition
  // and ContentAction.  |extension| may be NULL in tests.  If |error| is empty,
  // the translation was successful and the returned rule is internally
  // consistent.
  std::unique_ptr<const ContentRule> CreateRule(
      const Extension* extension,
      const std::map<std::string, ContentPredicateFactory*>&
          predicate_factories,
      const api::events::Rule& api_rule,
      std::string* error);

  // True if this object is managing the rules for |context|.
  bool ManagingRulesForBrowserContext(content::BrowserContext* context);

  // True if |condition| matches on |tab|.
  static bool EvaluateConditionForTab(const ContentCondition* condition,
                                      content::WebContents* tab);

  std::set<raw_ptr<const ContentRule, SetExperimental>> GetMatchingRules(
      content::WebContents* tab) const;

  // Evaluates the conditions for |tab| based on the tab state and matching CSS
  // selectors.
  void EvaluateConditionsForTab(content::WebContents* tab);

  // Returns true if a rule created by |extension| should be evaluated for an
  // incognito renderer.
  bool ShouldEvaluateExtensionRulesForIncognitoRenderer(
      const Extension* extension) const;

  using ExtensionIdRuleIdPair = std::pair<extensions::ExtensionId, std::string>;
  using RulesMap =
      std::map<ExtensionIdRuleIdPair, std::unique_ptr<const ContentRule>>;

  RulesMap content_rules_;

  // Maps a WebContents to the set of rules that match on that WebContents.
  // This lets us call Revert as appropriate. Note that this is expected to have
  // a key-value pair for every WebContents the registry is tracking, even if
  // the value is the empty set.
  std::map<content::WebContents*,
           std::set<raw_ptr<const ContentRule, SetExperimental>>>
      active_rules_;

  // The evaluators responsible for creating predicates and tracking
  // predicate-related state.
  std::vector<std::unique_ptr<ContentPredicateEvaluator>> evaluators_;

  // Specifies what to do with evaluation requests.
  EvaluationDisposition evaluation_disposition_;

  // Contains WebContents which require rule evaluation. Only used while
  // |evaluation_disposition_| is DEFER.
  std::set<raw_ptr<content::WebContents, SetExperimental>> evaluation_pending_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
