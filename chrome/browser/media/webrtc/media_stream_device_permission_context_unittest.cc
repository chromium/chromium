// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/permissions/permission_request_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

class TestPermissionContext : public MediaStreamDevicePermissionContext {
 public:
  TestPermissionContext(Profile* profile,
                        const ContentSettingsType content_settings_type)
      : MediaStreamDevicePermissionContext(profile, content_settings_type) {}

  ~TestPermissionContext() override = default;
};

}  // anonymous namespace

// TODO(raymes): many tests in MediaStreamDevicesControllerTest should be
// converted to tests in this file.
class MediaStreamDevicePermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 public:
  MediaStreamDevicePermissionContextTests(
      const MediaStreamDevicePermissionContextTests&) = delete;
  MediaStreamDevicePermissionContextTests& operator=(
      const MediaStreamDevicePermissionContextTests&) = delete;

 protected:
  MediaStreamDevicePermissionContextTests() = default;

  void TestInsecureQueryingUrl(ContentSettingsType content_settings_type) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL insecure_url("http://www.example.com");
    GURL secure_url("https://www.example.com");

    // Check that there is no saved content settings.
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                      insecure_url.DeprecatedGetOriginAsURL(),
                                      content_settings_type));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(secure_url.DeprecatedGetOriginAsURL(),
                                      insecure_url.DeprecatedGetOriginAsURL(),
                                      content_settings_type));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                      secure_url.DeprecatedGetOriginAsURL(),
                                      content_settings_type));

    EXPECT_EQ(
        PermissionStatus::DENIED,
        permission_context
            .GetPermissionStatus(
                content::PermissionDescriptorUtil::
                    CreatePermissionDescriptorForPermissionType(
                        permissions::PermissionUtil::
                            ContentSettingsTypeToPermissionType(
                                content_settings_type)),
                nullptr /* render_frame_host */, insecure_url, insecure_url)
            .status);

    EXPECT_EQ(PermissionStatus::DENIED,
              permission_context
                  .GetPermissionStatus(
                      content::PermissionDescriptorUtil::
                          CreatePermissionDescriptorForPermissionType(
                              permissions::PermissionUtil::
                                  ContentSettingsTypeToPermissionType(
                                      content_settings_type)),
                      nullptr /* render_frame_host */, insecure_url, secure_url)
                  .status);
  }

  void TestSecureQueryingUrl(ContentSettingsType content_settings_type) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL secure_url("https://www.example.com");

    // Check that there is no saved content settings.
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(secure_url.DeprecatedGetOriginAsURL(),
                                      secure_url.DeprecatedGetOriginAsURL(),
                                      content_settings_type));

    EXPECT_EQ(PermissionStatus::ASK,
              permission_context
                  .GetPermissionStatus(
                      content::PermissionDescriptorUtil::
                          CreatePermissionDescriptorForPermissionType(
                              permissions::PermissionUtil::
                                  ContentSettingsTypeToPermissionType(
                                      content_settings_type)),
                      nullptr /* render_frame_host */, secure_url, secure_url)
                  .status);
  }

  void TestUseFakeUiSwitch(ContentSettingsType content_setting_type,
                           bool use_deny_switch) {
    GURL secure_url("https://www.example.com");
    TestPermissionContext permission_context(profile(), content_setting_type);

    EXPECT_EQ(PermissionStatus::ASK,
              permission_context
                  .GetPermissionStatus(
                      content::PermissionDescriptorUtil::
                          CreatePermissionDescriptorForPermissionType(
                              permissions::PermissionUtil::
                                  ContentSettingsTypeToPermissionType(
                                      content_setting_type)),
                      nullptr /* render_frame_host */, secure_url, secure_url)
                  .status);

    if (use_deny_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kUseFakeUIForMediaStream, "deny");
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeUIForMediaStream);
    }

    EXPECT_EQ(
        use_deny_switch ? PermissionStatus::DENIED : PermissionStatus::GRANTED,
        permission_context
            .GetPermissionStatus(
                content::PermissionDescriptorUtil::
                    CreatePermissionDescriptorForPermissionType(
                        permissions::PermissionUtil::
                            ContentSettingsTypeToPermissionType(
                                content_setting_type)),
                nullptr /* render_frame_host */, secure_url, secure_url)
            .status);

    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kUseFakeUIForMediaStream);
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_ANDROID)
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
#else
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
#endif
  }
};

// MEDIASTREAM_MIC permission status should be ask for insecure origin to
// accommodate the usage case of Flash.
TEST_F(MediaStreamDevicePermissionContextTests, TestMicInsecureQueryingUrl) {
  TestInsecureQueryingUrl(ContentSettingsType::MEDIASTREAM_MIC);
}

// MEDIASTREAM_CAMERA permission status should be ask for insecure origin to
// accommodate the usage case of Flash.
TEST_F(MediaStreamDevicePermissionContextTests, TestCameraInsecureQueryingUrl) {
  TestInsecureQueryingUrl(ContentSettingsType::MEDIASTREAM_CAMERA);
}

// MEDIASTREAM_MIC permission status should be ask for Secure origin.
TEST_F(MediaStreamDevicePermissionContextTests, TestMicSecureQueryingUrl) {
  TestSecureQueryingUrl(ContentSettingsType::MEDIASTREAM_MIC);
}

// MEDIASTREAM_CAMERA permission status should be ask for Secure origin.
TEST_F(MediaStreamDevicePermissionContextTests, TestCameraSecureQueryingUrl) {
  TestSecureQueryingUrl(ContentSettingsType::MEDIASTREAM_CAMERA);
}

TEST_F(MediaStreamDevicePermissionContextTests, TestMicUseFakeUiSwitch) {
  TestUseFakeUiSwitch(ContentSettingsType::MEDIASTREAM_MIC,
                      false /* use_deny_switch */);
}

TEST_F(MediaStreamDevicePermissionContextTests, TestCameraUseFakeUiSwitch) {
  TestUseFakeUiSwitch(ContentSettingsType::MEDIASTREAM_CAMERA,
                      false /* use_deny_switch */);
}

TEST_F(MediaStreamDevicePermissionContextTests, TestMicUseFakeUiSwitchDeny) {
  TestUseFakeUiSwitch(ContentSettingsType::MEDIASTREAM_MIC,
                      true /* use_deny_switch */);
}

TEST_F(MediaStreamDevicePermissionContextTests, TestCameraUseFakeUiSwitchDeny) {
  TestUseFakeUiSwitch(ContentSettingsType::MEDIASTREAM_CAMERA,
                      true /* use_deny_switch */);
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(MediaStreamDevicePermissionContextTests,
       AppMediaPermissionAutoApproved) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder(
          "Test App", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .AddAPIPermission("audioCapture")
          .Build();

  extensions::ExtensionRegistry::Get(profile())->AddEnabled(app);
  extensions::RendererStartupHelperFactory::GetForBrowserContext(profile())
      ->OnExtensionLoaded(*app);

  extensions::SetViewType(
      web_contents(), extensions::mojom::ViewType::kExtensionBackgroundPage);

  GURL app_url = app->GetResourceURL("index.html");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(app_url);

  content::ChildProcessId process_id = main_rfh()->GetProcess()->GetID();
  extensions::ProcessMap::Get(profile())->Insert(app->id(), process_id);

  TestPermissionContext permission_context(
      profile(), ContentSettingsType::MEDIASTREAM_MIC);

  permissions::PermissionRequestID request_id(
      main_rfh(), permissions::PermissionRequestID::RequestLocalId(1));

  auto request_data = std::make_unique<permissions::PermissionRequestData>(
      blink::mojom::PermissionDescriptor::New(), request_id,
      /*user_gesture=*/true, url::Origin::Create(app_url).GetURL(),
      url::Origin::Create(app_url).GetURL());

  base::RunLoop run_loop;
  permission_context.DecidePermission(
      std::move(request_data),
      base::BindOnce(
          [](base::OnceClosure quit_closure, content::PermissionResult result) {
            EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, result.status);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
