// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_WEB_CONTENT_HANDLER_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_WEB_CONTENT_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/allocator/partition_allocator/pointers/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/supervised_user/web_approvals_manager.h"
#include "components/supervised_user/core/browser/web_content_handler.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Chrome Ash specific implementation of web content handler.
class WebContentHandlerImpl : public supervised_user::WebContentHandler {
 public:
  explicit WebContentHandlerImpl(content::WebContents& web_contents);

  WebContentHandlerImpl(const WebContentHandlerImpl&) = delete;
  WebContentHandlerImpl& operator=(const WebContentHandlerImpl&) = delete;
  ~WebContentHandlerImpl() override;

  // supervised_user::WebContentHandler:
  void RequestLocalApproval(const GURL& url,
                            const std::u16string& child_display_name,
                            const gfx::ImageSkia& favicon,
                            ApprovalRequestInitiatedCallback callback) override;

 private:
  void OnLocalApprovalRequestCompleted(
      supervised_user::SupervisedUserSettingsService& settings_service,
      const GURL& url,
      base::TimeTicks start_time,
      crosapi::mojom::ParentAccessResultPtr result);

  // Helpers for private method testing.
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalApprovedChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalDeclinedChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalCanceledChromeOSTest);
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalErrorChromeOSTest);

  const raw_ref<content::WebContents> web_contents_;
  base::WeakPtrFactory<WebContentHandlerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_WEB_CONTENT_HANDLER_IMPL_H_
