// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

namespace {

// The number of minutes a NOTIFICATION Issue stays in the IssueManager
// before it is auto-dismissed.
constexpr int kNotificationAutoDismissMins = 1;

// The number of minutes a WARNING Issue stays in the IssueManager before it
// is auto-dismissed.
constexpr int kWarningAutoDismissMins = 5;

}  // namespace

// static
base::TimeDelta IssueManager::GetAutoDismissTimeout(
    const IssueInfo& issue_info) {
  if (issue_info.is_blocking)
    return base::TimeDelta();

  switch (issue_info.severity) {
    case IssueInfo::Severity::NOTIFICATION:
      return base::TimeDelta::FromMinutes(kNotificationAutoDismissMins);
    case IssueInfo::Severity::WARNING:
      return base::TimeDelta::FromMinutes(kWarningAutoDismissMins);
    case IssueInfo::Severity::FATAL:
      NOTREACHED() << "FATAL issues should be blocking";
      return base::TimeDelta();
  }
  NOTREACHED();
  return base::TimeDelta();
}

IssueManager::IssueManager()
    : top_issue_(nullptr),
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {}

IssueManager::~IssueManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IssueManager::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& issues_map =
      issue_info.is_blocking ? blocking_issues_ : non_blocking_issues_;
  for (const auto& key_value_pair : issues_map) {
    const auto& issue = key_value_pair.second->issue;
    if (issue.info() == issue_info)
      return;
  }

  Issue issue(issue_info);
  std::unique_ptr<base::CancelableClosure> cancelable_dismiss_cb;
  base::TimeDelta timeout = GetAutoDismissTimeout(issue_info);
  if (!timeout.is_zero()) {
    cancelable_dismiss_cb =
        std::make_unique<base::CancelableClosure>(base::Bind(
            &IssueManager::ClearIssue, base::Unretained(this), issue.id()));
    task_runner_->PostDelayedTask(FROM_HERE, cancelable_dismiss_cb->callback(),
                                  timeout);
  }

  issues_map.emplace(issue.id(), std::make_unique<IssueManager::Entry>(
                                     issue, std::move(cancelable_dismiss_cb)));
  MaybeUpdateTopIssue();
}

void IssueManager::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (non_blocking_issues_.erase(issue_id) || blocking_issues_.erase(issue_id))
    MaybeUpdateTopIssue();
}

void IssueManager::ClearNonBlockingIssues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (non_blocking_issues_.empty())
    return;

  non_blocking_issues_.clear();
  MaybeUpdateTopIssue();
}

void IssueManager::RegisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  DCHECK(!issues_observers_.HasObserver(observer));

  issues_observers_.AddObserver(observer);
  if (top_issue_)
    observer->OnIssue(*top_issue_);
}

void IssueManager::UnregisterObserver(IssuesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  issues_observers_.RemoveObserver(observer);
}

IssueManager::Entry::Entry(
    const Issue& issue,
    std::unique_ptr<base::CancelableClosure> cancelable_dismiss_callback)
    : issue(issue),
      cancelable_dismiss_callback(std::move(cancelable_dismiss_callback)) {}

IssueManager::Entry::~Entry() = default;

void IssueManager::MaybeUpdateTopIssue() {
  const Issue* new_top_issue = nullptr;
  // Select the first blocking issue in the list of issues.
  // If there are none, simply select the first issue in the list.
  if (!blocking_issues_.empty()) {
    new_top_issue = &blocking_issues_.begin()->second->issue;
  } else if (!non_blocking_issues_.empty()) {
    new_top_issue = &non_blocking_issues_.begin()->second->issue;
  }

  // If we've found a new top issue, then report it via the observer.
  if (new_top_issue != top_issue_) {
    top_issue_ = new_top_issue;
    for (auto& observer : issues_observers_) {
      if (top_issue_)
        observer.OnIssue(*top_issue_);
      else
        observer.OnIssuesCleared();
    }
  }
}

}  // namespace media_router
