// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_TEST_SUPPORT_TEST_PWC_PERMISSION_DELEGATE_H_
#define CHROME_BROWSER_PWC_TEST_SUPPORT_TEST_PWC_PERMISSION_DELEGATE_H_

#include <optional>
#include <utility>

#include "chrome/browser/pwc/pwc_permission_delegate.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace pwc {

class TestPwcPermissionDelegate : public PwcPermissionDelegate {
 public:
  TestPwcPermissionDelegate();
  ~TestPwcPermissionDelegate() override;

  std::optional<content::PermissionResult> GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      ContentSettingsType type) override;

  void set_result(std::optional<content::PermissionResult> result) {
    result_ = std::move(result);
  }

  content::RenderFrameHost* last_rfh() const;
  content::GlobalRenderFrameHostId last_rfh_id() const { return last_rfh_id_; }
  std::optional<ContentSettingsType> last_type() const { return last_type_; }
  int call_count() const { return call_count_; }

 private:
  content::GlobalRenderFrameHostId last_rfh_id_;
  std::optional<ContentSettingsType> last_type_;
  int call_count_ = 0;
  std::optional<content::PermissionResult> result_ =
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED);
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_TEST_SUPPORT_TEST_PWC_PERMISSION_DELEGATE_H_
