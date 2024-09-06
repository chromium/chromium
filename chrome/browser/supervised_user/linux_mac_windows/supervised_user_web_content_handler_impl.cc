// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_web_content_handler_impl.h"

#include "base/notreached.h"
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
  // Method unsupposted on Desktop.
  NOTREACHED();
}
