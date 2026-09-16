// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_PERMISSION_DELEGATE_H_
#define CHROME_BROWSER_PWC_PWC_PERMISSION_DELEGATE_H_

#include <optional>

#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/permission_result.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace pwc {

// Component-provided delegate that handles and overrides Web platform
// permission requests for a PrivilegedWebContents.
//
// Implemented by the blessed component (e.g. //chrome/browser/glic) to map
// component-specific preferences or policies to permission decisions.
class PwcPermissionDelegate {
 public:
  virtual ~PwcPermissionDelegate() = default;

  // Returns the permission status override for `type` requested by
  // `render_frame_host`. Returns std::nullopt if this component does not
  // handle or override the given permission type.
  virtual std::optional<content::PermissionResult> GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      ContentSettingsType type) = 0;
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_PERMISSION_DELEGATE_H_
