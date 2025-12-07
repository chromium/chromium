// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/chrome_supervised_user_web_content_handler_base.h"
#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_result_observer.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}  // namespace content

namespace supervised_user {
class UrlFormatter;
}  // namespace supervised_user

class ParentAccessView;
class ParentAccessDialogResultObserver;

// Windows / Mac / Linux implementation of web content handler, which
// forces unsupported methods to fail.
class SupervisedUserWebContentHandlerImpl
    : public ChromeSupervisedUserWebContentHandlerBase {
 public:
  SupervisedUserWebContentHandlerImpl(content::WebContents* web_contents,
                                      content::FrameTreeNodeId frame_id,
                                      int64_t interstitial_navigation_id);
  SupervisedUserWebContentHandlerImpl(
      const SupervisedUserWebContentHandlerImpl&) = delete;
  SupervisedUserWebContentHandlerImpl& operator=(
      const SupervisedUserWebContentHandlerImpl&) = delete;
  ~SupervisedUserWebContentHandlerImpl() override;

  // ChromeSupervisedUserWebContentHandlerBase implementation:
  void RequestLocalApproval(
      const GURL& url,
      const std::u16string& child_display_name,
      const supervised_user::UrlFormatter& url_formatter,
      const supervised_user::FilteringBehaviorReason& filtering_reason,
      ApprovalRequestInitiatedCallback callback) override;
  void MaybeCloseLocalApproval() override;

  content::WebContents* GetObserverContentsForTesting() {
    CHECK(dialog_web_contents_observer_);
    return dialog_web_contents_observer_->GetWebContentsForTesting();
  }

  base::WeakPtr<ParentAccessView> GetWeakParentAccessViewForTesting() {
    return weak_parent_access_view_;
  }

 private:
  void StartObservingPacpContents(content::WebContents* contents);

  void CompleteUrlApprovalAndCloseOrUpdateDialog(
      const GURL& target_url,
      base::TimeTicks start_time,
      supervised_user::LocalApprovalResult result,
      std::optional<supervised_user::LocalWebApprovalErrorType> error_type);

  void CloseDialog();
  void DisplayErrorMessageInDialog();

  // Aborts the local web approval flow with an Error result and closes any open
  // parent approval dialog.
  void AbortUrlApprovalDialogOnTimeout();

  // Stops WebContents observation by the `dialog_web_contents_observer_`
  // and resets the unique pointer.
  void ResetDialogResultContentObserver();

  bool IsLocalApprovalInProgress();

  std::unique_ptr<ParentAccessDialogResultObserver>
      dialog_web_contents_observer_;
  base::WeakPtr<ParentAccessView> weak_parent_access_view_;
  base::WeakPtrFactory<SupervisedUserWebContentHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
