// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/test_support/test_pwc_permission_delegate.h"

#include "content/public/browser/render_frame_host.h"

namespace pwc {

TestPwcPermissionDelegate::TestPwcPermissionDelegate() = default;

TestPwcPermissionDelegate::~TestPwcPermissionDelegate() = default;

std::optional<content::PermissionResult>
TestPwcPermissionDelegate::GetPermissionStatus(
    content::RenderFrameHost* render_frame_host,
    ContentSettingsType type) {
  last_rfh_id_ = render_frame_host ? render_frame_host->GetGlobalId()
                                   : content::GlobalRenderFrameHostId();
  last_type_ = type;
  call_count_++;
  return result_;
}

content::RenderFrameHost* TestPwcPermissionDelegate::last_rfh() const {
  return content::RenderFrameHost::FromID(last_rfh_id_);
}

}  // namespace pwc
