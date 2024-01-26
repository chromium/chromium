// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_ASH_H_

#include "chrome/browser/extensions/api/identity/launch_web_auth_flow_delegate.h"

namespace extensions {

class LaunchWebAuthFlowDelegateAsh : public LaunchWebAuthFlowDelegate {
 public:
  // LaunchWebAuthFlowDelegate:
  void GetOptionalWindowBounds(
      Profile* profile,
      const std::string& extension_id,
      base::OnceCallback<void(std::optional<gfx::Rect>)> callback) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_ASH_H_
