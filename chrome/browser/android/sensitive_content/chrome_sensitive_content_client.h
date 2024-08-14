// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SENSITIVE_CONTENT_CHROME_SENSITIVE_CONTENT_CLIENT_H_
#define CHROME_BROWSER_ANDROID_SENSITIVE_CONTENT_CHROME_SENSITIVE_CONTENT_CLIENT_H_

#include "components/sensitive_content/sensitive_content_client.h"
#include "components/sensitive_content/sensitive_content_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace sensitive_content {

class ChromeSensitiveContentClient
    : public SensitiveContentClient,
      public content::WebContentsUserData<ChromeSensitiveContentClient> {
 public:
  explicit ChromeSensitiveContentClient(content::WebContents* web_contents);

  ChromeSensitiveContentClient(const ChromeSensitiveContentClient&) = delete;
  ChromeSensitiveContentClient& operator=(const ChromeSensitiveContentClient&) =
      delete;
  ~ChromeSensitiveContentClient() override;

  void SetContentSensitivity(bool content_is_sensitive) override;

 private:
  SensitiveContentManager manager_;
};

}  // namespace sensitive_content

#endif  // CHROME_BROWSER_ANDROID_SENSITIVE_CONTENT_CHROME_SENSITIVE_CONTENT_CLIENT_H_
