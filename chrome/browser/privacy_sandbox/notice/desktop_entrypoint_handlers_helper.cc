// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers_helper.h"

#include "chrome/common/webui_url_constants.h"
namespace privacy_sandbox {
// static
bool IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:
  // about:blank is valid.
  if (url.IsAboutBlank()) {
    return true;
  }
  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL)) {
    return true;
  }
  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL)) {
    return true;
  }
  // Only a Chrome controlled New Tab Page is valid. Third party NTP is still
  // Chrome controlled, but is without Google branding.
  if (url == GURL(chrome::kChromeUINewTabPageURL) ||
      url == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    return true;
  }
  return false;
}
}  // namespace privacy_sandbox
