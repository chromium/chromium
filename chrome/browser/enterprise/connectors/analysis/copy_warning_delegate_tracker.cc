// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/copy_warning_delegate_tracker.h"

#include <optional>

#include "base/memory/ptr_util.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_connectors {

// static
void CopyWarningDelegateTracker::SetDelegate(
    content::WebContents* web_contents,
    ContentAnalysisDelegate* delegate) {
  if (!web_contents) {
    return;
  }
  CreateForWebContents(web_contents);
  auto* tracker = FromWebContents(web_contents);
  if (tracker->delegate_ && tracker->delegate_.get() != delegate) {
    auto* old_delegate = tracker->delegate_.get();
    tracker->delegate_ = nullptr;
    old_delegate->Cancel(/*warning=*/true);
    old_delegate->Delete();
  }
  tracker->delegate_ = delegate ? delegate->GetWeakPtr() : nullptr;
}

// static
void CopyWarningDelegateTracker::BypassAndClear(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* tracker = FromWebContents(web_contents);
  if (tracker && tracker->delegate_) {
    if (tracker->delegate_->BypassRequiresJustification()) {
      ContentAnalysisDialogDelegate::ShowForCopyJustification(
          web_contents, base::WrapUnique(tracker->delegate_.get()));
      tracker->delegate_ = nullptr;
      // Do not delete the delegate here, it is now owned by the dialog.
      return;
    }

    auto* delegate = tracker->delegate_.get();
    tracker->delegate_ = nullptr;
    delegate->BypassWarnings(std::nullopt);
    delegate->Delete();
  }
}

// static
void CopyWarningDelegateTracker::CancelAndClear(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* tracker = FromWebContents(web_contents);
  if (tracker && tracker->delegate_) {
    auto* delegate = tracker->delegate_.get();
    tracker->delegate_ = nullptr;
    delegate->Cancel(/*warning=*/true);
    delegate->Delete();
  }
}

// static
void CopyWarningDelegateTracker::ClearIfMatches(
    content::WebContents* web_contents,
    ContentAnalysisDelegate* delegate) {
  if (!web_contents) {
    return;
  }
  auto* tracker = FromWebContents(web_contents);
  if (tracker && tracker->delegate_.get() == delegate) {
    tracker->delegate_ = nullptr;
  }
}

CopyWarningDelegateTracker::CopyWarningDelegateTracker(
    content::WebContents* web_contents)
    : content::WebContentsUserData<CopyWarningDelegateTracker>(*web_contents) {}

CopyWarningDelegateTracker::~CopyWarningDelegateTracker() {
  if (delegate_) {
    delegate_->Cancel(/*warning=*/true);
    delegate_->Delete();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CopyWarningDelegateTracker);

}  // namespace enterprise_connectors
