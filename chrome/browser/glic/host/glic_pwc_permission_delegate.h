// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_PWC_PERMISSION_DELEGATE_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_PWC_PERMISSION_DELEGATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/pwc/pwc_permission_delegate.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/permission_result.h"

class Profile;

namespace content {
class RenderFrameHost;
}

namespace glic {

// PwcPermissionDelegate implementation for Glic in GlicNoWebview mode.
// Handles permissions requested by the Glic guest web contents (e.g.
// microphone for Gemini Live, geolocation, clipboard) by checking Glic-specific
// user preferences and enterprise policies.
class GlicPwcPermissionDelegate : public pwc::PwcPermissionDelegate {
 public:
  explicit GlicPwcPermissionDelegate(Profile* profile);
  ~GlicPwcPermissionDelegate() override;

  GlicPwcPermissionDelegate(const GlicPwcPermissionDelegate&) = delete;
  GlicPwcPermissionDelegate& operator=(const GlicPwcPermissionDelegate&) =
      delete;

  std::optional<content::PermissionResult> GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      ContentSettingsType type) override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_PWC_PERMISSION_DELEGATE_H_
