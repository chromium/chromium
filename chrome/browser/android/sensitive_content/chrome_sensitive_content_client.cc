// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/sensitive_content/chrome_sensitive_content_client.h"

#include "content/public/browser/web_contents.h"

namespace sensitive_content {

ChromeSensitiveContentClient::ChromeSensitiveContentClient(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeSensitiveContentClient>(*web_contents),
      manager_(web_contents, this) {}

ChromeSensitiveContentClient::~ChromeSensitiveContentClient() = default;

void ChromeSensitiveContentClient::SetContentSensitivity(
    bool content_is_sensitive) {}

}  // namespace sensitive_content
