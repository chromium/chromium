// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

namespace shell_integration {

bool SetAsDefaultBrowser() {
  return false;
}

bool SetAsDefaultClientForScheme(const std::string& scheme) {
  return false;
}

std::u16string GetApplicationNameForScheme(const GURL& url) {
  return std::u16string();
}

DefaultWebClientState GetDefaultBrowser() {
  // Chrome is always the default system browser in Chrome OS. This is called
  // from ChromeAppDelegate::NewWindowContentsDelegate::OpenURLFromTab() where
  // we should navigate internally since we are the default browser rather than
  // call platform_util::OpenExternal().
  return IS_DEFAULT;
}

bool IsFirefoxDefaultBrowser() {
  return false;
}

DefaultWebClientState IsDefaultClientForScheme(const std::string& scheme) {
  return UNKNOWN_DEFAULT;
}

namespace internal {

DefaultWebClientSetPermission GetPlatformSpecificDefaultWebClientSetPermission(
    WebClientSetMethod method) {
  return SET_DEFAULT_NOT_ALLOWED;
}

}  // namespace internal

}  // namespace shell_integration
