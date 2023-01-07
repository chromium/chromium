// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_EVALUATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_EVALUATOR_H_

#include <map>
#include <vector>

#include "chrome/browser/extensions/api/declarative_content/content_predicate.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace extensions {

// Creates and manages instances of an associated ContentPredicate subclass and
// tracks the url and browser context state required to evaluate the predicates.
//
// A ContentPredicateEvaluator corresponds to a single attribute name across all
// chrome.declarativeContent.PageStateMatchers provided in all rules in the
// Declarative Content API. For example, given the rules:
//
// var rule1 = {
//   conditions: [
//     new chrome.declarativeContent.PageStateMatcher({
//       pageUrl: { hostEquals: 'www.google.com', schemes: ['https'] },
//       css: ['input[type=\'password\']']
//     })
//   ],
//   actions: [ new chrome.declarativeContent.ShowPageAction() ]
// };
//
// var rule2 = {
//   conditions: [
//     new chrome.declarativeContent.PageStateMatcher({
//       pageUrl: { hostEquals: 'www.example.com' },
//       css: ['a', 'image']
//     })
//   ],
//   actions: [ new chrome.declarativeContent.ShowPageAction() ]
// };
//
// The subclass of ContentPredicateEvaluator whose
// GetPredicateApiAttributeName() function returns "pageUrl" is responsible for
// creating and managing the predicates
// { hostEquals: 'www.google.com', schemes: ['https'] } and
// { hostEquals: 'www.example.com' }.
//
// The subclass of ContentPredicateEvaluator whose
// GetPredicateApiAttributeName() function returns "css" is responsible for
// creating and managing the predicates ['input[type=\'password\']'] and
// ['a', 'image'].
class ContentPredicateEvaluator : public ContentPredicateFactory {
 public:
  class Delegate;

  ContentPredicateEvaluator(const ContentPredicateEvaluator&) = delete;
  ContentPredicateEvaluator& operator=(const ContentPredicateEvaluator&) =
      delete;

  ~ContentPredicateEvaluator() override;

  // Returns the attribute name in the API for this evaluator's predicates.
  virtual std::string GetPredicateApiAttributeName() const = 0;

  // Notifies the evaluator that the grouped predicates should be tracked. This
  // function must always be called after creating a set of predicates. If the
  // predicates should be tracked, |predicates| must contain all the created
  // predicates. Otherwise, it will be called with an empty map and the created
  // predicates should not be tracked.
  virtual void TrackPredicates(
      const std::map<const void*,
                     std::vector<const ContentPredicate*>>& predicates) = 0;

  // Notifies the evaluator that it should stop tracking the predicates
  // associated with |predicate_groups|.
  virtual void StopTrackingPredicates(
      const std::vector<const void*>& predicate_groups) = 0;

  // Requests that predicates be tracked for |contents|.
  virtual void TrackForWebContents(content::WebContents* contents) = 0;

  // Handles navigation of |contents|. We depend on the caller to notify us of
  // this event rather than having each evaluator listen to it, so that the
  // caller can coordinate evaluation with all the evaluators that respond to
  // it. If an evaluator listened and requested rule evaluation before another
  // evaluator received the notification, the first evaluator's predicates would
  // be evaluated based on the new URL while the other evaluator's conditions
  // would still be evaluated based on the previous URL.
  virtual void OnWebContentsNavigation(
      content::WebContents* contents,
      content::NavigationHandle* navigation_handle) = 0;

  // Applies the given content rules to |contents| when the render process
  // notifies that a tab has started or stopped matching certain conditions.
  virtual void OnWatchedPageChanged(
      content::WebContents* contents,
      const std::vector<std::string>& css_selectors) = 0;

  // Returns true if |predicate| evaluates to true on the state associated with
  // |tab|. It must be the case that predicate->GetEvaluator() == this object,
  // |predicate| was previously passed to TrackPredicates(), and
  // StopTrackingPredicates has not yet been called with the group containing
  // |predicate|.
  virtual bool EvaluatePredicate(const ContentPredicate* predicate,
                                 content::WebContents* tab) const = 0;

 protected:
  ContentPredicateEvaluator();
};

// Allows an evaluator to notify that predicate evaluation state has been
// updated, and determine whether it should manage predicates for a context.
class ContentPredicateEvaluator::Delegate {
 public:
  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  // Notifies that predicate evaluation state has been updated for
  // |contents|. This must be called whenever the URL or page state changes,
  // even if the value of the predicate evaluation itself doesn't change.
  // TODO(wittman): rename to something like NotifyPredicateStateUpdated.
  virtual void RequestEvaluation(content::WebContents* contents) = 0;

  // Returns true if the evaluator should manage condition state for
  // |context|.  TODO(wittman): rename to something like
  // ShouldManagePredicatesForBrowserContext.
  virtual bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) = 0;

 protected:
  Delegate();
  virtual ~Delegate();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_EVALUATOR_H_
