// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer_common.h"

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/referrer.h"

bool CanOpenPdfUrl(content::RenderFrameHost* render_frame_host,
                   const GURL& url,
                   const GURL& last_committed_url,
                   content::Referrer* referrer) {
  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          render_frame_host->GetProcess()->GetID(), url)) {
    return false;
  }

  *referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(last_committed_url,
                             network::mojom::ReferrerPolicy::kDefault));
  return true;
}
