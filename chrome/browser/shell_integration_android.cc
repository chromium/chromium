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

bool SetAsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED();
  return false;
}

DefaultWebClientSetPermission
GetPlatformSpecificDefaultWebClientSetPermission() {
  NOTIMPLEMENTED();
  return SET_DEFAULT_NOT_ALLOWED;
}

std::u16string GetApplicationNameForProtocol(const GURL& url) {
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

DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED();
  return UNKNOWN_DEFAULT;
}

}  // namespace shell_integration
