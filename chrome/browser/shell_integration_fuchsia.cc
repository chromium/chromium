// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1226243): Implement default-browser & protocol-handler
// integration.

#include "chrome/browser/shell_integration.h"

#include "base/notreached.h"

namespace shell_integration {

bool SetAsDefaultBrowser() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool SetAsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

DefaultWebClientSetPermission
GetPlatformSpecificDefaultWebClientSetPermission() {
  return SET_DEFAULT_UNATTENDED;
}

std::u16string GetApplicationNameForProtocol(const GURL& url) {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

DefaultWebClientState GetDefaultBrowser() {
  // TODO(crbug.com/1226243): Chromium is effectively the default until Fuchsia
  // implements a picker.
  return DefaultWebClientState::IS_DEFAULT;
}

bool IsFirefoxDefaultBrowser() {
  // TODO(crbug.com/1226243): Chromium is effectively the default until Fuchsia
  // implements a picker.
  return false;
}

DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  NOTIMPLEMENTED_LOG_ONCE();
  return DefaultWebClientState::UNKNOWN_DEFAULT;
}

}  // namespace shell_integration
