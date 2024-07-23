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
  // ContentSettingsType(s) of the embedder the permission depends on.
  std::set<ContentSettingsType> embedder_content_settings_type;
};

enum class EmbedderPolicy {
  kNoPolicy,
  kNoRequestingOrigin,
  kNoEmbedderOrigin,
  kBothEmbedderAndRequestingOrigin
};

struct PermissionRequestTestParam {
  std::string name;
  bool calls_allow;
  EmbedderPolicy embedder_policy;
  bool has_embedder_content_setting;
  bool expected_success;
};

const std::vector<PermissionRequestTestParam>&
GetDefaultPermissionRequestTestParams();

class ControlledFramePermissionRequestTestBase
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<PermissionRequestTestParam> {
 protected:
  // `ControlledFrameTestBase`:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  void RunTestAndVerify(const PermissionRequestTestCase& test_case,
                        const PermissionRequestTestParam& test_param,
                        std::optional<base::OnceCallback<std::string(bool)>>
                            get_expected_result_callback = std::nullopt);

 private:
  void SetUpPermissionRequestEventListener(
      content::RenderFrameHost* app_frame,
      const std::string& expected_permission_name,
      bool allow_permission);
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_PERMISSION_REQUEST_TEST_BASE_H_
