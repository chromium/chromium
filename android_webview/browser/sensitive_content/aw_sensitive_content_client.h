// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SENSITIVE_CONTENT_AW_SENSITIVE_CONTENT_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_SENSITIVE_CONTENT_AW_SENSITIVE_CONTENT_CLIENT_H_

#include "components/sensitive_content/sensitive_content_client.h"
#include "components/sensitive_content/sensitive_content_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class AwSensitiveContentClient
    : public SensitiveContentClient,
      public content::WebContentsUserData<AwSensitiveContentClient> {
 public:
  explicit AwSensitiveContentClient(content::WebContents* web_contents);

  AwSensitiveContentClient(const AwSensitiveContentClient&) = delete;
  AwSensitiveContentClient& operator=(const AwSensitiveContentClient&) = delete;
  ~AwSensitiveContentClient() override;

  void SetContentSensitivity(bool content_is_sensitive) override;

 private:
  SensitiveContentManager manager_;
};

}  // namespace sensitive_content

#endif  // ANDROID_WEBVIEW_BROWSER_SENSITIVE_CONTENT_AW_SENSITIVE_CONTENT_CLIENT_H_
