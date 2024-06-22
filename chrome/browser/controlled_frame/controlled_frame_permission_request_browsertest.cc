// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace controlled_frame {

namespace {
constexpr char kPermissionAllowedHost[] = "permission-allowed.com";
constexpr char kPermissionDisallowedHost[] = "permission-disllowed.com";
}  // namespace

class ControlledFramePermissionRequestTestBase
    : public ControlledFrameTestBase {
 protected:
  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }

  void SetUpPermissionRequestEventListener(content::RenderFrameHost* app_frame,
                                           bool allow_permission) {
    const std::string& handle_request_str = allow_permission ? "allow" : "deny";
    EXPECT_EQ("SUCCESS", content::EvalJs(app_frame, content::JsReplace(
                                                        R"(
      (function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame) {
          return 'FAIL: Could not find a controlledframe element.';
        }
        frame.addEventListener('permissionrequest', (e) => {
          e.request[$1]();
        });
        return 'SUCCESS'
      })();
    )",
                                                        handle_request_str)));
  }
};

class ControlledFramePermissionRequestMediaTest
    : public ControlledFramePermissionRequestTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ControlledFramePermissionRequestTestBase::SetUpCommandLine(command_line);
    command_line->AppendArg("--use-fake-device-for-media-stream");
  }

  void RequestMediaPermissionFromControlledFrame(
      content::RenderFrameHost* app_frame,
      bool request_audio,
      bool request_video,
      bool expect_audio_permission_allowed,
      bool expect_video_permission_allowed) {
    extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
    EXPECT_EQ("SUCCESS", content::EvalJs(web_view_guest->GetGuestMainFrame(),
                                         content::JsReplace(
                                             R"(
    (async function() {
      const constraints = { audio: $1, video: $2 };
      const expectAudioPermissionAllowed = $3;
      const expectVideoPermissionAllowed = $4;
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);

        const checkPermissionType =
            function(type, tracks, expectPermissionAllowed) {
          const hasTracks = tracks.length;
          if (expectPermissionAllowed != hasTracks) {
            const expectedPermissionStr =
                expectPermissionAllowed ? 'has' : 'does not have';
            const hasTrackStr = hasTracks ? 'has' : 'does not have';
            return 'FAIL: getUserMedia() ' + expectedPermissionStr + ' ' +
                type + ' stream permission, but ' + hasTrackStr + ' ' +
                type + ' tracks';
          }
          return 'SUCCESS';
        }

        let audioPermissionCheckResult = checkPermissionType(
            'audio', stream.getAudioTracks(), expectAudioPermissionAllowed);
        if (audioPermissionCheckResult != 'SUCCESS') {
          return audioPermissionCheckResult;
        }

        let videoPermissionCheckResult = checkPermissionType(
            'video', stream.getVideoTracks(), expectVideoPermissionAllowed);
        if (videoPermissionCheckResult != 'SUCCESS') {
          return videoPermissionCheckResult;
        }

        return 'SUCCESS';
      } catch (err) {
        if (!expectAudioPermissionAllowed && !expectVideoPermissionAllowed) {
          return 'SUCCESS';
        }
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();
  )",
                                             request_audio, request_video,
                                             expect_audio_permission_allowed,
                                             expect_video_permission_allowed)));
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraPermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       OnlyCameraPermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraPermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraPermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/false,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       MicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       OnlyMicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       MicrophonePermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       MicrophonePermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder().AddPermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kMicrophone, /*self=*/true,
          {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/false,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraAndMicrophonePermissionAllowed) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/true,
      /*expect_video_permission_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraAndMicrophonePermissionDenied) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionAllowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/false);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

IN_PROC_BROWSER_TEST_F(ControlledFramePermissionRequestMediaTest,
                       CameraAndMicrophonePermissionDisallowed) {
  web_app::IsolatedWebAppUrlInfo url_info = CreateAndInstallEmptyApp(
      web_app::ManifestBuilder()
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              {embedded_https_test_server().GetOrigin(kPermissionAllowedHost)})
          .AddPermissionsPolicy(
              blink::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true,
              {embedded_https_test_server().GetOrigin(
                  kPermissionAllowedHost)}));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL(kPermissionDisallowedHost,
                                                     "/index.html")));

  SetUpPermissionRequestEventListener(app_frame, /*allow_permission=*/true);
  RequestMediaPermissionFromControlledFrame(
      app_frame,
      /*request_audio=*/true,
      /*request_video=*/true,
      /*expect_audio_permission_allowed=*/false,
      /*expect_video_permission_allowed=*/false);
}

}  // namespace controlled_frame
