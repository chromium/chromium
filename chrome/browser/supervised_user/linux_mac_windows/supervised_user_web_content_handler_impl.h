// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/chrome_supervised_user_web_content_handler_base.h"

namespace content {
class WebContents;
}  // namespace content

namespace supervised_user {
class UrlFormatter;
}  // namespace supervised_user

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
  void RequestLocalApproval(const GURL& url,
                            const std::u16string& child_display_name,
                            const supervised_user::UrlFormatter& url_formatter,
                            ApprovalRequestInitiatedCallback callback) override;

 private:
  base::WeakPtrFactory<SupervisedUserWebContentHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
