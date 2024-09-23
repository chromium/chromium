// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_PERMISSION_REQUEST_TEST_BASE_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_PERMISSION_REQUEST_TEST_BASE_H_

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

namespace controlled_frame {

struct PermissionRequestTestCase {
  PermissionRequestTestCase();
  ~PermissionRequestTestCase();

  // Javascript to invoke and verify the permission request from the embedded
  // content.
  std::string test_script;
  // The name of the permission in the event.
  std::string permission_name;
  // Policy features the permission depends on.
  std::set<blink::mojom::PermissionsPolicyFeature> policy_features;
  // Corresponding ContentSettingsType(s) of the permission.
  std::set<ContentSettingsType> content_settings_type;
};

enum class EmbedderPolicy {
  kNoPolicy,
  kNoRequestingOrigin,
  kNoEmbedderOrigin,
  kBothEmbedderAndRequestingOrigin
};

enum class ContentSettingsState { kDefault, kAllow, kDeny };

struct PermissionRequestTestParam {
  std::string name;
  bool calls_allow;
  EmbedderPolicy embedder_policy;
  bool allowed_by_embedder_content_settings;
  ContentSettingsState embedded_origin_content_settings_state;
  bool expected_success;
};

const std::vector<PermissionRequestTestParam>&
GetDefaultPermissionRequestTestParams();

struct DisabledPermissionTestCase {
  DisabledPermissionTestCase();
  ~DisabledPermissionTestCase();

  // Script to request the permission.
  std::string request_script;
  // Policy features the permission depends on.
  std::set<blink::mojom::PermissionsPolicyFeature> policy_features;
  std::string success_result;
  std::string failure_result;
};

struct DisabledPermissionTestParam {
  std::string name;
  bool policy_features_enabled;
  bool iwa_expect_success;
  bool controlled_frame_expect_success;
};

const std::vector<DisabledPermissionTestParam>&
GetDefaultDisabledPermissionTestParams();

class ControlledFramePermissionRequestTestBase
    : public ControlledFrameTestBase {
 protected:
  // `ControlledFrameTestBase`:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  void VerifyEnabledPermission(
      const PermissionRequestTestCase& test_case,
      const PermissionRequestTestParam& test_param,
      std::optional<base::OnceCallback<std::string(bool)>>
          get_expected_result_callback = std::nullopt);

  void VerifyDisabledPermission(const DisabledPermissionTestCase& test_case,
                                const DisabledPermissionTestParam& test_param);

  void VerifyDisabledPermission(const DisabledPermissionTestCase& test_case,
                                const DisabledPermissionTestParam& test_param,
                                content::RenderFrameHost* app_frame,
                                content::RenderFrameHost* controlled_frame);

  std::pair<content::RenderFrameHost*, content::RenderFrameHost*>
  SetUpControlledFrame(const DisabledPermissionTestCase& test_case,
                       const DisabledPermissionTestParam& test_param);

 private:
  void SetUpPermissionRequestEventListener(
      content::RenderFrameHost* app_frame,
      const std::string& expected_permission_name,
      bool allow_permission);
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_PERMISSION_REQUEST_TEST_BASE_H_
