// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_result_observer.h"

#include "base/functional/callback.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace {
bool HasNavigatedToTerminalVerificationUrl(
    content::NavigationHandle* navigation_handle) {
  const GURL& handle_url = navigation_handle->GetURL();
  return navigation_handle->HasCommitted() && handle_url.is_valid() &&
         handle_url.spec().starts_with(supervised_user::kFamilyManagementUrl);
}
}  // namespace

ParentAccessDialogResultObserver::ParentAccessDialogResultObserver(
    LocalApprovalResultCallback url_approval_result_callback)
    : url_approval_result_callback_(std::move(url_approval_result_callback)) {}

ParentAccessDialogResultObserver::~ParentAccessDialogResultObserver() {
  // url_approval_result_callback_ might have been executed on
  // `DidFinishNavigation` if a response has been received from PACP.
  if (!url_approval_result_callback_.is_null()) {
    std::move(url_approval_result_callback_)
        // If there is not result set, then the dialog must have been closed
        // through a cancellation action (Close button, Escape press,
        // interstitial destruction).
        .Run(result_.value_or(supervised_user::LocalApprovalResult::kCanceled),
             error_type_);
  }
}

void ParentAccessDialogResultObserver::StartObserving(
    content::WebContents* contents) {
  CHECK(contents);
  Observe(contents);
}

void ParentAccessDialogResultObserver::StopObserving() {
  Observe(nullptr);
}

void ParentAccessDialogResultObserver::SetResultToError(
    supervised_user::LocalWebApprovalErrorType error_type) {
  error_type_ = error_type;
  result_ = supervised_user::LocalApprovalResult::kError;
}

void ParentAccessDialogResultObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& handle_url = navigation_handle->GetURL();
  std::optional<std::string> encoded_callback =
      supervised_user::MaybeGetPacpResultFromUrl(handle_url);

  if (!encoded_callback.has_value()) {
    // Early exit when the observed url is not the one containing the result.
    return;
  }

  if (encoded_callback.value().empty()) {
    // The `result` query param was empty.
    result_ = supervised_user::LocalApprovalResult::kError;
    error_type_ =
        supervised_user::LocalWebApprovalErrorType::kPacpEmptyResponse;
    return;
  }
  supervised_user::ParentAccessCallbackParsedResult callback_result =
      supervised_user::ParentAccessCallbackParsedResult::
          ParseParentAccessCallbackResult(encoded_callback.value());

  // Set the `result_` according to the parsed PACP callback we received.
  // This will be handled when the navigation completes.
  if (callback_result.GetError().has_value()) {
    result_ = supervised_user::LocalApprovalResult::kError;
    switch (callback_result.GetError().value()) {
      case supervised_user::ParentAccessWidgetError::kDecodingError:
        error_type_ = supervised_user::LocalWebApprovalErrorType::
            kFailureToDecodePacpResponse;
        break;
      case supervised_user::ParentAccessWidgetError::kParsingError:
        error_type_ = supervised_user::LocalWebApprovalErrorType::
            kFailureToParsePacpResponse;
        break;
      default:
        break;
    }
    return;
  }
  CHECK(callback_result.GetCallback().has_value());
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback = callback_result.GetCallback().value();

  switch (parent_access_callback.callback_case()) {
    case kids::platform::parentaccess::client::proto::ParentAccessCallback::
        CallbackCase::kOnParentVerified:
      result_ = supervised_user::LocalApprovalResult::kApproved;
      break;
    // TODO(crbug.com/385354582): Add support for the cancellation message
    // once PACP returns it for the approval flow.
    default:
      result_ = supervised_user::LocalApprovalResult::kError;
      error_type_ =
          supervised_user::LocalWebApprovalErrorType::kUnexpectedPacpResponse;
      break;
  }
}

void ParentAccessDialogResultObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!HasNavigatedToTerminalVerificationUrl(navigation_handle) ||
      !result_.has_value()) {
    return;
  }
  CHECK(!url_approval_result_callback_.is_null());
  std::move(url_approval_result_callback_).Run(result_.value(), error_type_);
}
