// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_WEB_CONTENTS_OBSERVER_H_

#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

// Observer for the web contents of the parent approval dialog.
// Observes the navigation within the PACP widget and extracts the parent
// approval result.
class ParentAccessDialogWebContentsObserver
    : public content::WebContentsObserver {
 public:
  using LocalApprovalResultCallback =
      base::OnceCallback<void(supervised_user::LocalApprovalResult)>;

  ParentAccessDialogWebContentsObserver(
      content::WebContents* web_contents,
      LocalApprovalResultCallback url_approval_result_callback);
  ~ParentAccessDialogWebContentsObserver() override;
  ParentAccessDialogWebContentsObserver(
      const ParentAccessDialogWebContentsObserver&) = delete;
  ParentAccessDialogWebContentsObserver& operator=(
      const ParentAccessDialogWebContentsObserver&) = delete;

  void StopObserving();

 private:
  // WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::optional<supervised_user::LocalApprovalResult> result_;
  LocalApprovalResultCallback url_approval_result_callback_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_WEB_CONTENTS_OBSERVER_H_
