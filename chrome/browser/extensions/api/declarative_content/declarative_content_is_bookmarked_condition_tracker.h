// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"

namespace base {
class Value;
}

namespace extensions {

// Tests the bookmarked state of the page.
class DeclarativeContentIsBookmarkedPredicate : public ContentPredicate {
 public:
  ~DeclarativeContentIsBookmarkedPredicate() override;

  bool IsIgnored() const override;

  bool is_bookmarked() const { return is_bookmarked_; }

  static std::unique_ptr<DeclarativeContentIsBookmarkedPredicate> Create(
      ContentPredicateEvaluator* evaluator,
      const Extension* extension,
      const base::Value& value,
      std::string* error);

  // ContentPredicate:
  ContentPredicateEvaluator* GetEvaluator() const override;

 private:
  DeclarativeContentIsBookmarkedPredicate(
      ContentPredicateEvaluator* evaluator,
      scoped_refptr<const Extension> extension,
      bool is_bookmarked);

  // Weak.
  ContentPredicateEvaluator* const evaluator_;

  scoped_refptr<const Extension> extension_;
  bool is_bookmarked_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedPredicate);
};

// Supports tracking of URL matches across tab contents in a browser context,
// and querying for the matching condition sets.
class DeclarativeContentIsBookmarkedConditionTracker
    : public ContentPredicateEvaluator,
      public bookmarks::BaseBookmarkModelObserver {
 public:
  DeclarativeContentIsBookmarkedConditionTracker(
      content::BrowserContext* context,
      Delegate* delegate);
  ~DeclarativeContentIsBookmarkedConditionTracker() override;

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

    void BookmarkAddedForUrl(const GURL& url);
    void BookmarkRemovedForUrls(const std::set<GURL>& urls);
    void UpdateState(bool request_evaluation_if_unchanged);

    bool is_url_bookmarked() {
      return is_url_bookmarked_;
    }

   private:
    bool IsCurrentUrlBookmarked();

    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    bool is_url_bookmarked_;
    const RequestEvaluationCallback request_evaluation_;
    const WebContentsDestroyedCallback web_contents_destroyed_;

    DISALLOW_COPY_AND_ASSIGN(PerWebContentsTracker);
  };

  // bookmarks::BookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(
      bookmarks::BookmarkModel* model) override;

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* tracker);

  // Updates the bookmark state of all per-WebContents trackers.
  void UpdateAllPerWebContentsTrackers();

  // Weak.
  Delegate* const delegate_;

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*, std::unique_ptr<PerWebContentsTracker>>
      per_web_contents_tracker_;

  // Count of the number of extensive bookmarks changes in progress (e.g. due to
  // sync). The rules need only be evaluated once after the extensive changes
  // are complete.
  int extensive_bookmark_changes_in_progress_;

  ScopedObserver<bookmarks::BookmarkModel, bookmarks::BookmarkModelObserver>
      scoped_bookmarks_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_
