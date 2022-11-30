// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_is_bookmarked_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"

namespace extensions {

std::vector<std::unique_ptr<ContentPredicateEvaluator>>
CreateDefaultContentPredicateEvaluators(
    content::BrowserContext* browser_context,
    ContentPredicateEvaluator::Delegate* delegate) {
  std::vector<std::unique_ptr<ContentPredicateEvaluator>> evaluators;
  evaluators.push_back(std::unique_ptr<ContentPredicateEvaluator>(
      new DeclarativeContentPageUrlConditionTracker(delegate)));
  evaluators.push_back(std::unique_ptr<ContentPredicateEvaluator>(
      new DeclarativeContentCssConditionTracker(delegate)));
  evaluators.push_back(std::unique_ptr<ContentPredicateEvaluator>(
      new DeclarativeContentIsBookmarkedConditionTracker(browser_context,
                                                         delegate)));
  return evaluators;
}

}  // namespace extensions
