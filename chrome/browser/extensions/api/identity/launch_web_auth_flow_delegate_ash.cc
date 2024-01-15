// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/launch_web_auth_flow_delegate_ash.h"

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

void LaunchWebAuthFlowDelegateAsh::GetOptionalWindowBounds(
    Profile* profile,
    const std::string& extension_id,
    base::OnceCallback<void(std::optional<gfx::Rect>)> callback) {
  // ODFS does not have windows itself. We want to use the Files app window
  // instead.
  if (extension_id == extension_misc::kODFSExtensionId) {
    std::move(callback).Run(
        ash::cloud_upload::CalculateAuthWindowBounds(profile));
    return;
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace extensions
