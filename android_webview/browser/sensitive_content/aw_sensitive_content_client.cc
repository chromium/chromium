// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/sensitive_content/aw_sensitive_content_client.h"

#include "content/public/browser/web_contents.h"

namespace sensitive_content {

AwSensitiveContentClient::AwSensitiveContentClient(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AwSensitiveContentClient>(*web_contents),
      manager_(web_contents, this) {}

AwSensitiveContentClient::~AwSensitiveContentClient() = default;

void AwSensitiveContentClient::SetContentSensitivity(
    bool content_is_sensitive) {}

}  // namespace sensitive_content
