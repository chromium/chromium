// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_web_contents_observer.h"

namespace {

constexpr char kParentAccessResultQueryParameter[] = "result";

constexpr char kPacpOriginUrlHost[] = "families.google.com";

bool CanExtractLocalApprovalResultFromUrl(const GURL& url) {
  return url.host().starts_with(kPacpOriginUrlHost) &&
         url.query().starts_with(kParentAccessResultQueryParameter);
}

bool HasNavigatedToTerminalVerificationUrl(
    content::NavigationHandle* navigation_handle) {
  const GURL& handle_url = navigation_handle->GetURL();
  return navigation_handle->HasCommitted() && handle_url.is_valid() &&
         handle_url.spec().starts_with(supervised_user::kFamilyManagementUrl);
}
}  // namespace

ParentAccessDialogWebContentsObserver::ParentAccessDialogWebContentsObserver(
    content::WebContents* web_contents,
    LocalApprovalResultCallback url_approval_result_callback)
    : content::WebContentsObserver(web_contents),
      url_approval_result_callback_(std::move(url_approval_result_callback)) {}

ParentAccessDialogWebContentsObserver::
    ~ParentAccessDialogWebContentsObserver() = default;

void ParentAccessDialogWebContentsObserver::StopObserving() {
  Observe(nullptr);
}

void ParentAccessDialogWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& handle_url = navigation_handle->GetURL();
  if (CanExtractLocalApprovalResultFromUrl(handle_url)) {
    // TODO(crbug.com/383997522): Extract the result and complete the flow.
    result_ = supervised_user::LocalApprovalResult::kMaxValue;
  }
}

void ParentAccessDialogWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!HasNavigatedToTerminalVerificationUrl(navigation_handle) ||
      !result_.has_value()) {
    return;
  }
  CHECK(!url_approval_result_callback_.is_null());
  std::move(url_approval_result_callback_).Run(result_.value());
  result_ = std::nullopt;
  url_approval_result_callback_.Reset();
}
