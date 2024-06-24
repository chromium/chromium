// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "url/origin.h"

namespace controlled_frame {

namespace {
constexpr char kPermissionAllowedHost[] = "permission-allowed.com";
constexpr char kPermissionDisallowedHost[] = "permission-disllowed.com";

struct PermissionRequestTestCase {
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

struct PermissionRequestTestParam {
  std::string name;
  bool calls_allow;
  bool has_policy_feature;
  bool matches_policy_origin;
  bool has_embedder_content_setting;
  std::string expected_result;
};

std::vector<PermissionRequestTestParam> kTestParams{
    {.name = "Succeeds",
     .calls_allow = true,
     .has_policy_feature = true,
     .matches_policy_origin = true,
     .has_embedder_content_setting = true,
     .expected_result = "SUCCESS"},
    {.name = "FailsBecauseNotAllow",
     .calls_allow = false,
     .has_policy_feature = true,
     .matches_policy_origin = true,
     .has_embedder_content_setting = true,
     .expected_result = "FAIL: NotAllowedError: Permission denied"},
    {.name = "FailsBecauseNoPermissionPolicy",
     .calls_allow = true,
     .has_policy_feature = false,
     .matches_policy_origin = true,
     .has_embedder_content_setting = true,
     .expected_result = "FAIL: NotAllowedError: Invalid state"},
    {.name = "FailsBecausePolicyOriginMismatch",
     .calls_allow = true,
     .has_policy_feature = true,
     .matches_policy_origin = false,
     .has_embedder_content_setting = true,
     .expected_result = "FAIL: NotAllowedError: Invalid state"},
    {.name = "FailsBecauseNoEmbedderContentSettings",
     .calls_allow = true,
     .has_policy_feature = true,
     .matches_policy_origin = true,
     .has_embedder_content_setting = false,
     .expected_result = "FAIL: NotAllowedError: Invalid state"},
};
}  // namespace

class ControlledFramePermissionRequestTest
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<PermissionRequestTestParam> {
 protected:
  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ControlledFrameTestBase::SetUpCommandLine(command_line);
    command_line->AppendArg("--use-fake-device-for-media-stream");
  }

  void SetUpPermissionRequestEventListener(
      content::RenderFrameHost* app_frame,
      const std::string& expected_permission_name,
      bool allow_permission) {
    const std::string& handle_request_str = allow_permission ? "allow" : "deny";
    EXPECT_EQ("SUCCESS",
              content::EvalJs(app_frame, content::JsReplace(
                                             R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('permissionrequest', (e) => {
          if (e.permission === $1) {
            e.request[$2]();
          }
        });
        return 'SUCCESS';
      })();
    )",
                                             expected_permission_name,
                                             handle_request_str)));
  }

  void RunTestAndVerify(const PermissionRequestTestCase& test_case,
                        const PermissionRequestTestParam& test_param) {
    // If the permission has no dependent permissions policy feature, then skip
    // the true negative permissions policy test cases.
    if (!test_param.has_policy_feature && test_case.policy_features.empty()) {
      return;
    }
    // If the permission has no dependent embedder content setting, then skip
    // the true negative embedder content settings test cases.
    if (!test_param.has_embedder_content_setting &&
        test_case.embedder_content_settings_type.empty()) {
      return;
    }

    web_app::ManifestBuilder manifest_builder = web_app::ManifestBuilder();
    if (test_param.has_policy_feature) {
      url::Origin policy_origin =
          embedded_https_test_server().GetOrigin(kPermissionAllowedHost);
      for (auto& policy_feature : test_case.policy_features) {
        manifest_builder.AddPermissionsPolicy(policy_feature, /*self=*/true,
                                              {policy_origin});
      }
    }

    web_app::IsolatedWebAppUrlInfo url_info =
        CreateAndInstallEmptyApp(manifest_builder);
    content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

    ASSERT_TRUE(CreateControlledFrame(
        app_frame,
        embedded_https_test_server().GetURL(test_param.matches_policy_origin
                                                ? kPermissionAllowedHost
                                                : kPermissionDisallowedHost,
                                            "/index.html")));

    extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
    ASSERT_TRUE(web_view_guest);
    content::RenderFrameHost* controlled_frame =
        web_view_guest->GetGuestMainFrame();

    SetUpPermissionRequestEventListener(app_frame, test_case.permission_name,
                                        test_param.calls_allow);

    if (test_param.has_embedder_content_setting) {
      // TODO(b/344910997): set embedder content settings.
    }

    EXPECT_EQ(test_param.expected_result,
              content::EvalJs(controlled_frame, test_case.test_script));
  }
};

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Camera) {
  PermissionRequestTestCase test_case;
  test_case.test_script = content::JsReplace(
      R"(
    (async function() {
      const constraints = { video: true };
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        if(stream.getVideoTracks().length > 0){
          return 'SUCCESS';
        }
        return 'FAIL: ' + stream.getVideoTracks().length + ' tracks';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )");
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kCamera});
  // TODO(b/344910997): Add embedder content settings.

  PermissionRequestTestParam test_param = GetParam();
  RunTestAndVerify(test_case, test_param);
}

IN_PROC_BROWSER_TEST_P(ControlledFramePermissionRequestTest, Microphone) {
  PermissionRequestTestCase test_case;
  test_case.test_script = content::JsReplace(
      R"(
    (async function() {
      const constraints = { audio: true };
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        if(stream.getAudioTracks().length > 0){
          return 'SUCCESS';
        }
        return 'FAIL: ' + stream.getAudioTracks().length + ' tracks';
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )");
  test_case.permission_name = "media";
  test_case.policy_features.insert(
      {blink::mojom::PermissionsPolicyFeature::kMicrophone});
  // TODO(b/344910997): Add embedder content settings.

  PermissionRequestTestParam test_param = GetParam();
  RunTestAndVerify(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFramePermissionRequestTest,
                         testing::ValuesIn(kTestParams),
                         [](const testing::TestParamInfo<
                             PermissionRequestTestParam>& info) {
                           return info.param.name;
                         });

}  // namespace controlled_frame
