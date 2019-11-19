// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

namespace {

const char kCssInvalidTypeOfParameter[] = "Attribute '%s' has an invalid type";

}  // namespace

//
// DeclarativeContentCssPredicate
//

DeclarativeContentCssPredicate::~DeclarativeContentCssPredicate() {
}

// static
std::unique_ptr<DeclarativeContentCssPredicate>
DeclarativeContentCssPredicate::Create(ContentPredicateEvaluator* evaluator,
                                       const base::Value& value,
                                       std::string* error) {
  std::vector<std::string> css_rules;
  const base::ListValue* css_rules_value = nullptr;
  if (value.GetAsList(&css_rules_value)) {
    for (size_t i = 0; i < css_rules_value->GetSize(); ++i) {
      std::string css_rule;
      if (!css_rules_value->GetString(i, &css_rule)) {
        *error = base::StringPrintf(kCssInvalidTypeOfParameter,
                                    declarative_content_constants::kCss);
        return std::unique_ptr<DeclarativeContentCssPredicate>();
      }
      css_rules.push_back(css_rule);
    }
  } else {
    *error = base::StringPrintf(kCssInvalidTypeOfParameter,
                                declarative_content_constants::kCss);
    return std::unique_ptr<DeclarativeContentCssPredicate>();
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
PerWebContentsTracker(
    content::WebContents* contents,
    const RequestEvaluationCallback& request_evaluation,
    const WebContentsDestroyedCallback& web_contents_destroyed)
    : WebContentsObserver(contents),
      request_evaluation_(request_evaluation),
      web_contents_destroyed_(web_contents_destroyed) {
}

DeclarativeContentCssConditionTracker::PerWebContentsTracker::
~PerWebContentsTracker() {
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnWebContentsNavigation(content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    // Within-page navigations don't change the set of elements that
    // exist, and we only support filtering on the top-level URL, so
    // this can't change which rules match.
    return;
  }

  // Top-level navigation produces a new document. Initially, the
  // document's empty, so no CSS rules match.  The renderer will send
  // an ExtensionHostMsg_OnWatchedPageChange later if any CSS rules
  // match.
  matching_css_selectors_.clear();
  request_evaluation_.Run(web_contents());
}

bool
DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PerWebContentsTracker, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OnWatchedPageChange,
                        OnWatchedPageChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
WebContentsDestroyed() {
  web_contents_destroyed_.Run(web_contents());
}

void
DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnWatchedPageChange(
    const std::vector<std::string>& css_selectors) {
  matching_css_selectors_.clear();
  matching_css_selectors_.insert(css_selectors.begin(), css_selectors.end());
  request_evaluation_.Run(web_contents());
}

//
// DeclarativeContentCssConditionTracker
//

DeclarativeContentCssConditionTracker::DeclarativeContentCssConditionTracker(
    Delegate* delegate)
    : delegate_(delegate) {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

DeclarativeContentCssConditionTracker::
~DeclarativeContentCssConditionTracker() {}

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
    auto loc = tracked_predicates_.find(group);
    if (loc == tracked_predicates_.end())
      continue;
    for (const DeclarativeContentCssPredicate* predicate : loc->second) {
      for (const std::string& selector : predicate->css_selectors()) {
        auto loc = watched_css_selector_predicate_count_.find(selector);
        DCHECK(loc != watched_css_selector_predicate_count_.end());
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
      base::Bind(&Delegate::RequestEvaluation, base::Unretained(delegate_)),
      base::Bind(
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

bool DeclarativeContentCssConditionTracker::EvaluatePredicate(
    const ContentPredicate* predicate,
    content::WebContents* tab) const {
  DCHECK_EQ(this, predicate->GetEvaluator());
  const DeclarativeContentCssPredicate* typed_predicate =
      static_cast<const DeclarativeContentCssPredicate*>(predicate);
  auto loc = per_web_contents_tracker_.find(tab);
  DCHECK(loc != per_web_contents_tracker_.end());
  const std::unordered_set<std::string>& matching_css_selectors =
      loc->second->matching_css_selectors();
  for (const std::string& predicate_css_selector :
           typed_predicate->css_selectors()) {
    if (!base::Contains(matching_css_selectors, predicate_css_selector))
      return false;
  }

  return true;
}

void DeclarativeContentCssConditionTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_RENDERER_PROCESS_CREATED, type);

  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  InstructRenderProcessIfManagingBrowserContext(process,
                                                GetWatchedCssSelectors());
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
  for (const std::pair<std::string, int>& selector_pair :
           watched_css_selector_predicate_count_) {
    selectors.push_back(selector_pair.first);
  }
  return selectors;
}

void DeclarativeContentCssConditionTracker::
InstructRenderProcessIfManagingBrowserContext(
    content::RenderProcessHost* process,
    std::vector<std::string> watched_css_selectors) {
  if (delegate_->ShouldManageConditionsForBrowserContext(
          process->GetBrowserContext())) {
    process->Send(new ExtensionMsg_WatchPages(watched_css_selectors));
  }
}

void DeclarativeContentCssConditionTracker::DeletePerWebContentsTracker(
    content::WebContents* contents) {
  DCHECK(base::Contains(per_web_contents_tracker_, contents));
  per_web_contents_tracker_.erase(contents);
}

}  // namespace extensions
