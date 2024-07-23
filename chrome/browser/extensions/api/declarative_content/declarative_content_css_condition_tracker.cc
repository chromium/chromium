// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/mojom/renderer.mojom.h"

namespace extensions {

namespace {

const char kCssInvalidTypeOfParameter[] = "Attribute '%s' has an invalid type";

}  // namespace

//
// DeclarativeContentCssPredicate
//

DeclarativeContentCssPredicate::~DeclarativeContentCssPredicate() = default;

// static
std::unique_ptr<DeclarativeContentCssPredicate>
DeclarativeContentCssPredicate::Create(ContentPredicateEvaluator* evaluator,
                                       const base::Value& value,
                                       std::string* error) {
  std::vector<std::string> css_rules;
  if (value.is_list()) {
    for (const base::Value& css_rule_value : value.GetList()) {
      if (!css_rule_value.is_string()) {
        *error = base::StringPrintf(kCssInvalidTypeOfParameter,
                                    declarative_content_constants::kCss);
        return nullptr;
      }
      css_rules.push_back(css_rule_value.GetString());
    }
  } else {
    *error = base::StringPrintf(kCssInvalidTypeOfParameter,
                                declarative_content_constants::kCss);
    return nullptr;
  }

  return !css_rules.empty()
             ? base::WrapUnique(
                   new DeclarativeContentCssPredicate(evaluator, css_rules))
             : std::unique_ptr<DeclarativeContentCssPredicate>();
}

ContentPredicateEvaluator*
DeclarativeContentCssPredicate::GetEvaluator() const {
  return evaluator_;
}

DeclarativeContentCssPredicate::DeclarativeContentCssPredicate(
    ContentPredicateEvaluator* evaluator,
    const std::vector<std::string>& css_selectors)
    : evaluator_(evaluator),
      css_selectors_(css_selectors) {
  DCHECK(!css_selectors.empty());
}

//
// PerWebContentsTracker
//

DeclarativeContentCssConditionTracker::PerWebContentsTracker::
    PerWebContentsTracker(content::WebContents* contents,
                          RequestEvaluationCallback request_evaluation,
                          WebContentsDestroyedCallback web_contents_destroyed)
    : WebContentsObserver(contents),
      request_evaluation_(std::move(request_evaluation)),
      web_contents_destroyed_(std::move(web_contents_destroyed)) {}

DeclarativeContentCssConditionTracker::PerWebContentsTracker::
    ~PerWebContentsTracker() = default;

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnWebContentsNavigation(content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    // Within-page navigations don't change the set of elements that
    // exist, and we only support filtering on the top-level URL, so
    // this can't change which rules match.
    return;
  }

  // Top-level navigation produces a new document. Initially, the
  // document's empty, so no CSS rules match.  The renderer will call
  // 'extensions::mojom::LocalFrameHost::WatchedPageChange()' later if any CSS
  // rules match.
  matching_css_selectors_.clear();
  request_evaluation_.Run(web_contents());
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
    OnWatchedPageChanged(const std::vector<std::string>& css_selectors) {
  matching_css_selectors_.clear();
  matching_css_selectors_.insert(css_selectors.begin(), css_selectors.end());
  request_evaluation_.Run(web_contents());
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
WebContentsDestroyed() {
  std::move(web_contents_destroyed_).Run(web_contents());
}

//
// DeclarativeContentCssConditionTracker
//

DeclarativeContentCssConditionTracker::DeclarativeContentCssConditionTracker(
    Delegate* delegate)
    : delegate_(delegate) {}

DeclarativeContentCssConditionTracker::
    ~DeclarativeContentCssConditionTracker() = default;

std::string DeclarativeContentCssConditionTracker::
GetPredicateApiAttributeName() const {
  return declarative_content_constants::kCss;
}

std::unique_ptr<const ContentPredicate>
DeclarativeContentCssConditionTracker::CreatePredicate(
    const Extension* extension,
    const base::Value& value,
    std::string* error) {
  return DeclarativeContentCssPredicate::Create(this, value, error);
}

void DeclarativeContentCssConditionTracker::TrackPredicates(
    const std::map<const void*, std::vector<const ContentPredicate*>>&
        predicates) {
  bool watched_selectors_updated = false;
  for (const auto& group_predicates_pair : predicates) {
    for (const ContentPredicate* predicate : group_predicates_pair.second) {
      DCHECK_EQ(this, predicate->GetEvaluator());
      const DeclarativeContentCssPredicate* typed_predicate =
          static_cast<const DeclarativeContentCssPredicate*>(predicate);
      tracked_predicates_[group_predicates_pair.first].push_back(
          typed_predicate);
      for (const std::string& selector : typed_predicate->css_selectors()) {
        if (watched_css_selector_predicate_count_[selector]++ == 0)
          watched_selectors_updated = true;
      }
    }
  }

  if (watched_selectors_updated)
    UpdateRenderersWatchedCssSelectors(GetWatchedCssSelectors());
}

void DeclarativeContentCssConditionTracker::StopTrackingPredicates(
    const std::vector<const void*>& predicate_groups) {
  bool watched_selectors_updated = false;
  for (const void* group : predicate_groups) {
    auto it = tracked_predicates_.find(group);
    if (it == tracked_predicates_.end())
      continue;
    for (const DeclarativeContentCssPredicate* predicate : it->second) {
      for (const std::string& selector : predicate->css_selectors()) {
        auto loc = watched_css_selector_predicate_count_.find(selector);
        CHECK(loc != watched_css_selector_predicate_count_.end(),
              base::NotFatalUntil::M130);
        if (--loc->second == 0) {
          watched_css_selector_predicate_count_.erase(loc);
          watched_selectors_updated = true;
        }
      }
    }
    tracked_predicates_.erase(group);
  }

  if (watched_selectors_updated)
    UpdateRenderersWatchedCssSelectors(GetWatchedCssSelectors());
}

void DeclarativeContentCssConditionTracker::TrackForWebContents(
    content::WebContents* contents) {
  per_web_contents_tracker_[contents] = std::make_unique<PerWebContentsTracker>(
      contents,
      base::BindRepeating(&Delegate::RequestEvaluation,
                          base::Unretained(delegate_)),
      base::BindOnce(
          &DeclarativeContentCssConditionTracker::DeletePerWebContentsTracker,
          base::Unretained(this)));
  // Note: the condition is always false until we receive OnWatchedPageChange,
  // so there's no need to evaluate it here.
}

void DeclarativeContentCssConditionTracker::OnWebContentsNavigation(
    content::WebContents* contents,
    content::NavigationHandle* navigation_handle) {
  DCHECK(base::Contains(per_web_contents_tracker_, contents));
  per_web_contents_tracker_[contents]->OnWebContentsNavigation(
      navigation_handle);
}

void DeclarativeContentCssConditionTracker::OnWatchedPageChanged(
    content::WebContents* contents,
    const std::vector<std::string>& css_selectors) {
  DCHECK(base::Contains(per_web_contents_tracker_, contents));
  per_web_contents_tracker_[contents]->OnWatchedPageChanged(css_selectors);
}

bool DeclarativeContentCssConditionTracker::EvaluatePredicate(
    const ContentPredicate* predicate,
    content::WebContents* tab) const {
  DCHECK_EQ(this, predicate->GetEvaluator());
  const DeclarativeContentCssPredicate* typed_predicate =
      static_cast<const DeclarativeContentCssPredicate*>(predicate);
  auto loc = per_web_contents_tracker_.find(tab);
  CHECK(loc != per_web_contents_tracker_.end(), base::NotFatalUntil::M130);
  const std::unordered_set<std::string>& matching_css_selectors =
      loc->second->matching_css_selectors();
  for (const std::string& predicate_css_selector :
           typed_predicate->css_selectors()) {
    if (!base::Contains(matching_css_selectors, predicate_css_selector))
      return false;
  }

  return true;
}

void DeclarativeContentCssConditionTracker::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  InstructRenderProcessIfManagingBrowserContext(host, GetWatchedCssSelectors());
}

void DeclarativeContentCssConditionTracker::
UpdateRenderersWatchedCssSelectors(
    const std::vector<std::string>& watched_css_selectors) {
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    InstructRenderProcessIfManagingBrowserContext(it.GetCurrentValue(),
                                                  watched_css_selectors);
  }
}

std::vector<std::string> DeclarativeContentCssConditionTracker::
GetWatchedCssSelectors() const {
  std::vector<std::string> selectors;
  selectors.reserve(watched_css_selector_predicate_count_.size());
  for (const std::pair<const std::string, int>& selector_pair :
       watched_css_selector_predicate_count_) {
    selectors.push_back(selector_pair.first);
  }
  return selectors;
}

void DeclarativeContentCssConditionTracker::
InstructRenderProcessIfManagingBrowserContext(
    content::RenderProcessHost* process,
    std::vector<std::string> watched_css_selectors) {
  content::BrowserContext* browser_context = process->GetBrowserContext();
  if (delegate_->ShouldManageConditionsForBrowserContext(browser_context)) {
    mojom::Renderer* renderer =
        RendererStartupHelperFactory::GetForBrowserContext(browser_context)
            ->GetRenderer(process);
    if (renderer)
      renderer->WatchPages(watched_css_selectors);
  }
}

void DeclarativeContentCssConditionTracker::DeletePerWebContentsTracker(
    content::WebContents* contents) {
  DCHECK(base::Contains(per_web_contents_tracker_, contents));
  per_web_contents_tracker_.erase(contents);
}

}  // namespace extensions
