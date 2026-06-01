// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_utils.h"

#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

bool IsAccessRestrictedInFrame(content::RenderFrameHost* rfh) {
  return rfh->GetLastCommittedOrigin().opaque() || rfh->IsCredentialless() ||
         rfh->IsNestedWithinFencedFrame() ||
         rfh->IsSandboxed(
             network::mojom::WebSandboxFlags::kStorageAccessByUserActivation) ||
         rfh->GetStorageKey().ForbidsUnpartitionedStorageAccess();
}
