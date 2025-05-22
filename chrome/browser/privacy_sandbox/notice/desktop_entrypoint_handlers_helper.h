// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_
#include "url/gurl.h"
namespace privacy_sandbox {
// Returns whether |url| is suitable to display the Privacy Sandbox prompt
// over. Only about:blank and certain chrome:// URLs are considered
// suitable.
bool IsUrlSuitableForPrompt(const GURL& url);
}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_HELPER_H_
