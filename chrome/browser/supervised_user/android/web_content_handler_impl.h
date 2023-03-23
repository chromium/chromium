// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEB_CONTENT_HANDLER_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEB_CONTENT_HANDLER_IMPL_H_

#include <string>

#include "base/allocator/partition_allocator/pointers/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/supervised_user/core/browser/web_content_handler.h"

enum class AndroidLocalWebApprovalFlowOutcome;

namespace content {
class WebContents;
}  // namespace content

// Android specific implementation of web content handler.
class WebContentHandlerImpl : public supervised_user::WebContentHandler {
 public:
  explicit WebContentHandlerImpl(content::WebContents& web_contents);

  WebContentHandlerImpl(const WebContentHandlerImpl&) = delete;
  WebContentHandlerImpl& operator=(const WebContentHandlerImpl&) = delete;
  ~WebContentHandlerImpl() override;

  // supervised_user::WebContentHandler:
  void RequestLocalApproval(const GURL& url,
                            const std::u16string& child_display_name,
                            ApprovalRequestInitiatedCallback callback) override;

 private:
  void OnLocalApprovalRequestCompleted(
      supervised_user::SupervisedUserSettingsService& settings_service,
      const GURL& url,
      base::TimeTicks start_time,
      AndroidLocalWebApprovalFlowOutcome request_outcome);

  // Helpers for private method testing.
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalDurationHistogramRejectionTest);
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalDurationHistogramApprovalTest);
  FRIEND_TEST_ALL_PREFIXES(WebContentHandlerImplTest,
                           LocalWebApprovalDurationHistogramCancellationTest);

  const raw_ref<content::WebContents> web_contents_;
  base::WeakPtrFactory<WebContentHandlerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEB_CONTENT_HANDLER_IMPL_H_
