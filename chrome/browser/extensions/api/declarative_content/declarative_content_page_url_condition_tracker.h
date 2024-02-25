// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Value;
}

namespace extensions {

class Extension;

// Tests the URL of a page against conditions specified with the
// URLMatcherConditionSet.
class DeclarativeContentPageUrlPredicate : public ContentPredicate {
 public:
  DeclarativeContentPageUrlPredicate(
      const DeclarativeContentPageUrlPredicate&) = delete;
  DeclarativeContentPageUrlPredicate& operator=(
      const DeclarativeContentPageUrlPredicate&) = delete;

  ~DeclarativeContentPageUrlPredicate() override;

  url_matcher::URLMatcherConditionSet* url_matcher_condition_set() const {
    return url_matcher_condition_set_.get();
  }

  static std::unique_ptr<DeclarativeContentPageUrlPredicate> Create(
      ContentPredicateEvaluator* evaluator,
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& value,
      std::string* error);

  // ContentPredicate:
  ContentPredicateEvaluator* GetEvaluator() const override;

 private:
  DeclarativeContentPageUrlPredicate(
      ContentPredicateEvaluator* evaluator,
      scoped_refptr<url_matcher::URLMatcherConditionSet>
          url_matcher_condition_set);

  // Weak.
  const raw_ptr<ContentPredicateEvaluator, DanglingUntriaged> evaluator_;

  scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_condition_set_;
};

// Supports tracking of URL matches across tab contents in a browser context,
// and querying for the matching condition sets.
class DeclarativeContentPageUrlConditionTracker
    : public ContentPredicateEvaluator {
 public:
  explicit DeclarativeContentPageUrlConditionTracker(Delegate* delegate);

  DeclarativeContentPageUrlConditionTracker(
      const DeclarativeContentPageUrlConditionTracker&) = delete;
  DeclarativeContentPageUrlConditionTracker& operator=(
      const DeclarativeContentPageUrlConditionTracker&) = delete;

  ~DeclarativeContentPageUrlConditionTracker() override;

  // ContentPredicateEvaluator:
  std::string GetPredicateApiAttributeName() const override;
  std::unique_ptr<const ContentPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error) override;
  void TrackPredicates(
      const std::map<const void*, std::vector<const ContentPredicate*>>&
          predicates) override;
  void StopTrackingPredicates(
      const std::vector<const void*>& predicate_groups) override;
  void TrackForWebContents(content::WebContents* contents) override;
  void OnWebContentsNavigation(
      content::WebContents* contents,
      content::NavigationHandle* navigation_handle) override;
  void OnWatchedPageChanged(
      content::WebContents* contents,
      const std::vector<std::string>& css_selectors) override;
  bool EvaluatePredicate(const ContentPredicate* predicate,
                         content::WebContents* tab) const override;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  class PerWebContentsTracker : public content::WebContentsObserver {
   public:
    using RequestEvaluationCallback =
        base::RepeatingCallback<void(content::WebContents*)>;
    using WebContentsDestroyedCallback =
        base::OnceCallback<void(content::WebContents*)>;

    PerWebContentsTracker(content::WebContents* contents,
                          url_matcher::URLMatcher* url_matcher,
                          RequestEvaluationCallback request_evaluation,
                          WebContentsDestroyedCallback web_contents_destroyed);

    PerWebContentsTracker(const PerWebContentsTracker&) = delete;
    PerWebContentsTracker& operator=(const PerWebContentsTracker&) = delete;

    ~PerWebContentsTracker() override;

    void UpdateMatchesForCurrentUrl(bool request_evaluation_if_unchanged);

    const std::set<base::MatcherStringPattern::ID>& matches() {
      return matches_;
    }

   private:
    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    raw_ptr<url_matcher::URLMatcher> url_matcher_;
    const RequestEvaluationCallback request_evaluation_;
    WebContentsDestroyedCallback web_contents_destroyed_;

    std::set<base::MatcherStringPattern::ID> matches_;
  };

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* tracker);

  void UpdateMatchesForAllTrackers();

  // Weak.
  const raw_ptr<Delegate> delegate_;

  // Matches URLs for all WebContents.
  url_matcher::URLMatcher url_matcher_;

  // Grouped predicates tracked by this object.
  std::map<const void*,
           std::vector<raw_ptr<const DeclarativeContentPageUrlPredicate,
                               VectorExperimental>>>
      tracked_predicates_;

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*, std::unique_ptr<PerWebContentsTracker>>
      per_web_contents_tracker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_
