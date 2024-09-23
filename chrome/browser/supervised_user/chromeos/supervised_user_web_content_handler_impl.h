// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/chrome_supervised_user_web_content_handler_base.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class Profile;

namespace favicon {
class LargeIconService;
}  // namespace favicon

namespace supervised_user {
class UrlFormatter;
}  // namespace supervised_user

class SupervisedUserFaviconRequestHandler;

// Chrome OS specific implementation of web content handler.
class SupervisedUserWebContentHandlerImpl
    : public ChromeSupervisedUserWebContentHandlerBase {
 public:
  SupervisedUserWebContentHandlerImpl(
      content::WebContents* web_contents,
      const GURL& url,
      favicon::LargeIconService& large_icon_service,
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
  void OnLocalApprovalRequestCompleted(
      supervised_user::SupervisedUserSettingsService& settings_service,
      const GURL& url,
      base::TimeTicks start_time,
      crosapi::mojom::ParentAccessResultPtr result);

  // Helpers for private method testing.
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserWebContentHandlerImplTest,
                           LocalWebApprovalApprovedChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserWebContentHandlerImplTest,
                           LocalWebApprovalDeclinedChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserWebContentHandlerImplTest,
                           LocalWebApprovalCanceledChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserWebContentHandlerImplTest,
                           LocalWebApprovalErrorChromeOSTest);

  std::unique_ptr<SupervisedUserFaviconRequestHandler> favicon_handler_;
  const raw_ref<Profile> profile_;
  base::WeakPtrFactory<SupervisedUserWebContentHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_WEB_CONTENT_HANDLER_IMPL_H_
