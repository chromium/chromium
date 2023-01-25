// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "chrome/browser/shell_integration.h"

namespace shell_integration {

// TODO: crbug/115375 to track implementation for following methods.
bool SetAsDefaultBrowser() {
  NOTIMPLEMENTED();
  return false;
}

bool SetAsDefaultClientForScheme(const std::string& scheme) {
  NOTIMPLEMENTED();
  return false;
}

std::u16string GetApplicationNameForScheme(const GURL& url) {
  NOTIMPLEMENTED();
  return std::u16string();
}

DefaultWebClientState GetDefaultBrowser() {
  NOTIMPLEMENTED();
  return UNKNOWN_DEFAULT;
}

bool IsFirefoxDefaultBrowser() {
  return false;
}

DefaultWebClientState IsDefaultClientForScheme(const std::string& scheme) {
  NOTIMPLEMENTED();
  return UNKNOWN_DEFAULT;
}

namespace internal {

DefaultWebClientSetPermission GetPlatformSpecificDefaultWebClientSetPermission(
    WebClientSetMethod method) {
  NOTIMPLEMENTED();
  return SET_DEFAULT_NOT_ALLOWED;
}

}  // namespace internal

}  // namespace shell_integration
