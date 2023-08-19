// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/platform_verification_chromeos.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace platform_verification {
namespace {

// Whether platform verification is permitted by the user-configurable content
// setting.
bool IsPermittedByContentSettings(content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(
                 blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                 render_frame_host) == blink::mojom::PermissionStatus::GRANTED;
}

// Whether platform verification is permitted by the profile type. Currently
// platform verification is disabled for incognito and guest profiles.
bool IsPermittedByProfileType(content::RenderFrameHost* render_frame_host) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  return !profile->IsOffTheRecord() && !profile->IsGuestSession();
}

}  // namespace

bool PerformBrowserChecks(content::RenderFrameHost* render_frame_host) {
  GURL url = render_frame_host->GetLastCommittedOrigin().GetURL();

  if (!url.is_valid()) {
    return false;
  }

  if (!IsPermittedByProfileType(render_frame_host)) {
    return false;
  }

  if (!IsPermittedByContentSettings(render_frame_host)) {
    return false;
  }

  return true;
}

}  // namespace platform_verification
