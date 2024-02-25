// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_H_

#include <optional>

#include "base/functional/callback.h"
#include "ui/gfx/geometry/rect.h"

class Profile;

namespace extensions {

// An interface to delegate some behavior of chrome.identity.launchWebAuthFlow.
class LaunchWebAuthFlowDelegate {
 public:
  LaunchWebAuthFlowDelegate() = default;
  virtual ~LaunchWebAuthFlowDelegate() = default;

  LaunchWebAuthFlowDelegate(const LaunchWebAuthFlowDelegate&) = delete;
  LaunchWebAuthFlowDelegate& operator=(const LaunchWebAuthFlowDelegate&) =
      delete;

  // Called when LaunchWebAuthFlow may show a popup auth dialog. Allows the
  // delegate to provide window bounds for the new window.
  virtual void GetOptionalWindowBounds(
      Profile* profile,
      const std::string& extension_id,
      base::OnceCallback<void(std::optional<gfx::Rect>)> callback) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_LAUNCH_WEB_AUTH_FLOW_DELEGATE_H_
