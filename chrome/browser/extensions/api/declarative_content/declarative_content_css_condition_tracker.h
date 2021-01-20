// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Value;
}

namespace extensions {

class Extension;

// Tests whether all the specified CSS selectors match on the page.
class DeclarativeContentCssPredicate : public ContentPredicate {
 public:
  ~DeclarativeContentCssPredicate() override;

  const std::vector<std::string>& css_selectors() const {
    return css_selectors_;
  }

  static std::unique_ptr<DeclarativeContentCssPredicate> Create(
      ContentPredicateEvaluator* evaluator,
      const base::Value& value,
      std::string* error);

  // ContentPredicate:
  ContentPredicateEvaluator* GetEvaluator() const override;

 private:
  DeclarativeContentCssPredicate(ContentPredicateEvaluator* evaluator,
                                 const std::vector<std::string>& css_selectors);

  // Weak.
  ContentPredicateEvaluator* const evaluator_;
  std::vector<std::string> css_selectors_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssPredicate);
};

// Supports watching of CSS selectors to across tab contents in a browser
// context, and querying for the matching CSS selectors for a context.
class DeclarativeContentCssConditionTracker
    : public ContentPredicateEvaluator,
      public content::NotificationObserver {
 public:
  explicit DeclarativeContentCssConditionTracker(Delegate* delegate);
  ~DeclarativeContentCssConditionTracker() override;

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
  bool EvaluatePredicate(const ContentPredicate* predicate,
                         content::WebContents* tab) const override;

 private:
  // Monitors CSS selector matching state on one WebContents.
  class PerWebContentsTracker : public content::WebContentsObserver {
   public:
    using RequestEvaluationCallback =
        base::Callback<void(content::WebContents*)>;
    using WebContentsDestroyedCallback =
        base::Callback<void(content::WebContents*)>;

    PerWebContentsTracker(
        content::WebContents* contents,
        const RequestEvaluationCallback& request_evaluation,
        const WebContentsDestroyedCallback& web_contents_destroyed);
    ~PerWebContentsTracker() override;

    void OnWebContentsNavigation(content::NavigationHandle* navigation_handle);

    const std::unordered_set<std::string>& matching_css_selectors() const {
      return matching_css_selectors_;
    }

   private:
    // content::WebContentsObserver overrides.
    bool OnMessageReceived(const IPC::Message& message) override;
    void WebContentsDestroyed() override;

    void OnWatchedPageChange(const std::vector<std::string>& css_selectors);

    const RequestEvaluationCallback request_evaluation_;
    const WebContentsDestroyedCallback web_contents_destroyed_;

    // We use a hash_set for maximally efficient lookup.
    std::unordered_set<std::string> matching_css_selectors_;

    DISALLOW_COPY_AND_ASSIGN(PerWebContentsTracker);
  };

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Informs renderer processes of a new set of watched CSS selectors.
  void UpdateRenderersWatchedCssSelectors(
      const std::vector<std::string>& watched_css_selectors);

  // Returns the current list of watched CSS selectors.
  std::vector<std::string> GetWatchedCssSelectors() const;

  // If the renderer process is associated with our browser context, tells it
  // what page attributes to watch for using an ExtensionMsg_WatchPages.
  void InstructRenderProcessIfManagingBrowserContext(
      content::RenderProcessHost* process,
      std::vector<std::string> watched_css_selectors);

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* contents);

  // Weak.
  Delegate* delegate_;

  // Maps the CSS selectors to watch to the number of predicates specifying
  // them.
  std::map<std::string, int> watched_css_selector_predicate_count_;

  // Grouped predicates tracked by this object.
  std::map<const void*, std::vector<const DeclarativeContentCssPredicate*>>
      tracked_predicates_;

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*, std::unique_ptr<PerWebContentsTracker>>
      per_web_contents_tracker_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_
