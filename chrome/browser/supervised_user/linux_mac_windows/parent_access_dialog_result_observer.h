// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_RESULT_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_RESULT_OBSERVER_H_

#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Observer for the web contents of the parent approval dialog.
// Observes the navigation within the PACP widget and extracts the parent
// approval result.
class ParentAccessDialogResultObserver : public content::WebContentsObserver {
 public:
  using LocalApprovalResultCallback = base::OnceCallback<void(
      supervised_user::LocalApprovalResult,
      std::optional<supervised_user::LocalWebApprovalErrorType>)>;

  explicit ParentAccessDialogResultObserver(
      LocalApprovalResultCallback url_approval_result_callback);
  // The destructor records metrics on the approval's outcome for certain
  // outcomes (cancellations, error cases).
  ~ParentAccessDialogResultObserver() override;
  ParentAccessDialogResultObserver(const ParentAccessDialogResultObserver&) =
      delete;
  ParentAccessDialogResultObserver& operator=(
      const ParentAccessDialogResultObserver&) = delete;

  void StartObserving(content::WebContents* contents);
  void StopObserving();

  // Helper that sets the results to Error, in case we fail to load
  // and observe the content from the PACP widget.
  void SetResultToError(supervised_user::LocalWebApprovalErrorType error_type);

  content::WebContents* GetWebContentsForTesting() { return web_contents(); }

 private:
  // WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::optional<supervised_user::LocalApprovalResult> result_;
  std::optional<supervised_user::LocalWebApprovalErrorType> error_type_;
  LocalApprovalResultCallback url_approval_result_callback_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_DIALOG_RESULT_OBSERVER_H_
