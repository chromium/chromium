// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_web_content_handler_impl.h"

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

SupervisedUserWebContentHandlerImpl::SupervisedUserWebContentHandlerImpl(
    content::WebContents* web_contents,
    content::FrameTreeNodeId frame_id,
    int64_t interstitial_navigation_id)
    : ChromeSupervisedUserWebContentHandlerBase(web_contents,
                                                frame_id,
                                                interstitial_navigation_id) {}

SupervisedUserWebContentHandlerImpl::~SupervisedUserWebContentHandlerImpl() =
    default;

void SupervisedUserWebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const supervised_user::UrlFormatter& url_formatter,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(base::FeatureList::IsEnabled(supervised_user::kLocalWebApprovals));
  CHECK(web_contents_);

  auto target_url = url_formatter.FormatUrl(url);
  weak_parent_access_view_ =
      ParentAccessView::ShowParentAccessDialog(web_contents_, target_url);
  // TODO(crbug.com/383997522): Track the new web contents created by the above
  // call to obtain the result of the parents' approval.
  // TODO(crbug.com/383997522): Dismiss the dialog.

  // Runs the `callback` to inform the caller that the flow initiation was
  // successful.
  std::move(callback).Run(true);
}
